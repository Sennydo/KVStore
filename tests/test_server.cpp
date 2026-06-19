#include "server.h"
#include <arpa/inet.h>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <random>
#include <set>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

#define CHECK(cond) \
    do { if (!(cond)) { std::cerr << "FAIL: " #cond " (" << __FILE__ << ":" << __LINE__ << ")\n"; std::exit(1); } } while(0)

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static int make_client(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    CHECK(fd >= 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    CHECK(connect(fd, (sockaddr*)&addr, sizeof(addr)) == 0);
    return fd;
}

// Encode a command as a RESP array of bulk strings:
//   {"SET","foo","bar"} -> *3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\nbar\r\n
static std::string resp(const std::vector<std::string>& args) {
    std::string out = "*" + std::to_string(args.size()) + "\r\n";
    for (const auto& a : args)
        out += "$" + std::to_string(a.size()) + "\r\n" + a + "\r\n";
    return out;
}

static void send_all(int fd, const std::string& s) {
    size_t sent = 0;
    while (sent < s.size()) {
        ssize_t n = send(fd, s.data() + sent, s.size() - sent, 0);
        CHECK(n > 0);
        sent += n;
    }
}

// A single reply may arrive split across multiple TCP segments, so read until
// the buffer holds at least `expectedLen` bytes, then compare. A short read
// loop like this is what makes the assertion deterministic rather than racy.
static std::string read_reply(int fd, size_t expectedLen) {
    std::string buf;
    char chunk[256];
    while (buf.size() < expectedLen) {
        ssize_t n = recv(fd, chunk, sizeof(chunk), 0);
        CHECK(n > 0);   // 0 = peer closed before sending full reply
        buf.append(chunk, n);
    }
    return buf;
}

// Read exactly one RESP reply, framing by type rather than by a known length.
// Needed where the reply size isn't known in advance (a GET under contention
// can return a miss or any of several bulk strings). Reading by structure means
// we consume one reply and leave the socket positioned cleanly for the next.
static std::string read_one_reply(int fd) {
    auto readByte = [&](char& c) {
        ssize_t n = recv(fd, &c, 1, 0);
        CHECK(n > 0);
    };
    auto readUntilCRLF = [&](std::string& s) {
        // append bytes until s ends with \r\n
        while (s.size() < 2 || s[s.size()-2] != '\r' || s[s.size()-1] != '\n') {
            char c; readByte(c); s.push_back(c);
        }
    };

    std::string reply;
    char type; readByte(type); reply.push_back(type);

    if (type == '+' || type == '-' || type == ':') {
        // simple string / error / integer: one line, terminated by CRLF
        readUntilCRLF(reply);
    } else if (type == '$') {
        // bulk string: <len>\r\n then len bytes then \r\n, or $-1\r\n for null
        readUntilCRLF(reply);                          // finish the length line
        // parse the length out of what we just read (between '$' and '\r')
        std::string lenStr = reply.substr(1, reply.size() - 3);
        long long len = std::stoll(lenStr);
        if (len >= 0) {
            std::string body;
            body.resize(len + 2);                      // payload + trailing CRLF
            size_t got = 0;
            while (got < body.size()) {
                ssize_t n = recv(fd, &body[got], body.size() - got, 0);
                CHECK(n > 0);
                got += n;
            }
            reply += body;
        }
    } else {
        CHECK(false);   // unexpected reply type
    }
    return reply;
}

// Send one command, assert the full reply equals `want`.
static void expect(int fd, const std::vector<std::string>& cmd, const std::string& want) {
    send_all(fd, resp(cmd));
    std::string got = read_reply(fd, want.size());
    CHECK(got == want);
}

// RAII wrapper: starts the server in a background thread, stops it on destruction.
struct TestServer {
    Server server;
    std::thread thread;
    int port;

    explicit TestServer(int p) : server(p, 4), port(p) {
        thread = std::thread([this] { server.start(); });
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    ~TestServer() {
        server.stop();
        thread.join();
    }
};

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

void test_startup() {
    TestServer ts(19001);
    int fd = make_client(ts.port);
    expect(fd, {"PING"}, "+PONG\r\n");
    close(fd);
    std::cout << "PASS test_startup\n";
}

void test_set_get_del() {
    TestServer ts(19002);
    int fd = make_client(ts.port);

    expect(fd, {"SET", "foo", "bar"}, "+OK\r\n");      // store a value
    expect(fd, {"GET", "foo"}, "$3\r\nbar\r\n");       // read it back (hit)
    expect(fd, {"GET", "missing"}, "$-1\r\n");         // miss -> RESP null
    expect(fd, {"DEL", "foo"}, ":1\r\n");              // deletes one
    expect(fd, {"DEL", "foo"}, ":0\r\n");              // already gone
    expect(fd, {"GET", "foo"}, "$-1\r\n");             // confirmed gone

    close(fd);
    std::cout << "PASS test_set_get_del\n";
}

void test_bad_arity() {
    TestServer ts(19003);
    int fd = make_client(ts.port);

    // GET with no key is a protocol-level error; the server replies with a
    // RESP error frame (-...) rather than crashing or hanging.
    send_all(fd, resp({"GET"}));
    char chunk[256]{};
    ssize_t n = recv(fd, chunk, sizeof(chunk), 0);
    CHECK(n > 0);
    CHECK(chunk[0] == '-');   // RESP error prefix

    close(fd);
    std::cout << "PASS test_bad_arity\n";
}

void test_pipelined() {
    TestServer ts(19004);
    int fd = make_client(ts.port);

    // Two commands sent in one write. The server must parse and answer both
    // from a single buffer -- this exercises the INCOMPLETE/consumed loop in
    // handleClient, not just one-command-per-packet.
    send_all(fd, resp({"SET", "a", "1"}) + resp({"GET", "a"}));
    std::string want = "+OK\r\n$1\r\n1\r\n";
    std::string got = read_reply(fd, want.size());
    CHECK(got == want);

    close(fd);
    std::cout << "PASS test_pipelined\n";
}

void test_concurrent_clients() {
    TestServer ts(19005);
    const int N = 6;
    std::vector<std::thread> threads;
    std::atomic<int> passed{0};

    // Each client writes to its own key, then reads it back. Distinct keys
    // means they land on different shards -- this is the concurrency path the
    // sharded store exists for.
    for (int i = 0; i < N; ++i) {
        threads.emplace_back([i, &passed, &ts] {
            int fd = make_client(ts.port);
            std::string key = "client_" + std::to_string(i);
            std::string val = "v" + std::to_string(i);

            send_all(fd, resp({"SET", key, val}));
            std::string okWant = "+OK\r\n";
            if (read_reply(fd, okWant.size()) != okWant) { close(fd); return; }

            send_all(fd, resp({"GET", key}));
            std::string getWant = "$" + std::to_string(val.size()) + "\r\n" + val + "\r\n";
            if (read_reply(fd, getWant.size()) == getWant) ++passed;

            close(fd);
        });
    }
    for (auto& t : threads) t.join();
    CHECK(passed == N);
    std::cout << "PASS test_concurrent_clients (" << N << " clients)\n";
}

// Many threads pound the SAME few keys with interleaved SET/GET/DEL. With
// distinct keys (test_concurrent_clients) the shards never collide, so the
// locks are never contended; this test is the opposite -- it forces real
// contention on a shared_mutex and checks nothing impossible comes back.
//
// We can't predict what a racing GET returns, but we CAN bound it: every SET
// writes one of a small known set of values, so any GET reply must be either a
// miss ($-1) or one of those exact bulk strings. A torn or garbage read fails
// that check. That invariant is what proves the locking, not a fixed expected
// value.
void test_same_key_contention() {
    TestServer ts(19006);
    const int N = 8;                 // threads
    const int ITERS = 500;           // ops per thread
    const int NKEYS = 4;             // tiny keyspace -> guaranteed collisions

    // The only values any writer will ever store. A GET must return one of
    // these (as a RESP bulk string) or a miss -- never anything else.
    const std::vector<std::string> vals = {"alpha", "beta", "gamma", "delta"};

    // Precompute the legal RESP replies for a GET so the worker can compare
    // against a set rather than re-encode on every read.
    std::set<std::string> legalGetReplies;
    legalGetReplies.insert("$-1\r\n");                       // miss
    for (const auto& v : vals)
        legalGetReplies.insert("$" + std::to_string(v.size()) + "\r\n" + v + "\r\n");

    std::atomic<int> violations{0};

    auto worker = [&](int seed) {
        std::mt19937 rng(seed);
        std::uniform_int_distribution<int> keyDist(0, NKEYS - 1);
        std::uniform_int_distribution<int> valDist(0, (int)vals.size() - 1);
        std::uniform_int_distribution<int> opDist(0, 2);     // 0=SET 1=GET 2=DEL

        int fd = make_client(ts.port);
        for (int i = 0; i < ITERS; ++i) {
            std::string key = "k" + std::to_string(keyDist(rng));
            int op = opDist(rng);

            if (op == 0) {
                expect(fd, {"SET", key, vals[valDist(rng)]}, "+OK\r\n");
            } else if (op == 2) {
                // DEL returns :1 (existed) or :0 (already gone). Under a race
                // either is legal, so accept both rather than asserting one.
                send_all(fd, resp({"DEL", key}));
                std::string got = read_reply(fd, 4);         // ":1\r\n" / ":0\r\n"
                if (got != ":1\r\n" && got != ":0\r\n") ++violations;
            } else {
                // GET: read one full RESP reply, then check it's in the legal
                // set. This is the real assertion -- a torn value lands here.
                send_all(fd, resp({"GET", key}));
                std::string got = read_one_reply(fd);
                if (legalGetReplies.find(got) == legalGetReplies.end())
                    ++violations;
            }
        }
        close(fd);
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < N; ++i) threads.emplace_back(worker, i + 1);
    for (auto& t : threads) t.join();

    CHECK(violations == 0);
    std::cout << "PASS test_same_key_contention (" << N << " threads, "
              << NKEYS << " keys)\n";
}

// Send a single command one byte at a time, with a tiny pause between bytes so
// each lands in its own recv(). This forces the server's parse loop to hit
// INCOMPLETE on nearly every pass and buffer the fragments until the whole
// command has arrived -- the real-TCP path where a command is split across
// segments. If the server answered correctly only because commands happened to
// arrive whole, this test fails.
void test_fragmented_write() {
    TestServer ts(19007);
    int fd = make_client(ts.port);

    std::string cmd = resp({"SET", "frag", "value"});
    for (char c : cmd) {
        send_all(fd, std::string(1, c));
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    // Despite arriving in 20-odd separate segments, the server must assemble
    // one command and reply once.
    CHECK(read_reply(fd, 5) == "+OK\r\n");

    // And the store actually holds the value -- the fragments reassembled in
    // order, not just "a reply came back".
    expect(fd, {"GET", "frag"}, "$5\r\nvalue\r\n");

    close(fd);
    std::cout << "PASS test_fragmented_write\n";
}

int main() {
    test_startup();
    test_set_get_del();
    test_bad_arity();
    test_pipelined();
    test_concurrent_clients();
    test_same_key_contention();
    test_fragmented_write();
    std::cout << "All tests passed.\n";
    return 0;
}