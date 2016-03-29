#include "scy/http/server.h"
#include "scy/idler.h"


using std::cout;
using std::cerr;
using std::endl;


namespace scy {
// namespace http {


class BasicResponder: public http::ServerResponder
    /// Basic server responder (make echo?)
{
public:
    BasicResponder(http::ServerConnection& conn) :
        http::ServerResponder(conn)
    {
        DebugL << "Creating" << endl;
    }

    void onRequest(http::Request& request, http::Response& response)
    {
        DebugL << "On complete" << endl;

        response.setContentLength(14);  // headers will be auto flushed

        connection().send("hello universe", 14);
        connection().close();
    }
};


class BasicEchoResponder: public http::ServerResponder
    /// Basic echo server responder
{
public:
    BasicEchoResponder(http::ServerConnection& conn) :
        http::ServerResponder(conn)
    {
        DebugL << "Creating" << endl;
    }

    virtual void onPayload(const MutableBuffer& body)
    {
        DebugL << "On payload: " << body.size() << endl;

        // gotPayload = true;

        // Echo the request back to the client
        connection().send(body.cstr(), body.size());
    }
};


struct RandomDataSource: public Idler
{
    PacketSignal signal;

    void init()
    {
        start(std::bind(&RandomDataSource::onIdle, this));
    }

    void onIdle()
    {
        RawPacket packet("hello", 5);
        signal.emit(this, packet);
    }
};


class ChunkedResponder: public http::ServerResponder
    /// Chunked responder that broadcasts random data to the client.
{
public:
    RandomDataSource dataSource;
    bool gotHeaders;
    bool gotRequest;
    bool gotClose;

    ChunkedResponder(http::ServerConnection& conn) :
        http::ServerResponder(conn),
        gotHeaders(false),
        gotRequest(false),
        gotClose(false)
    {
        //conn.Outgoing.attach(new http::ChunkedAdapter(conn)); //"text/html"
        //conn.Outgoing.attachSource(&dataSource.signal, false);
        //dataSource.signal += sdelegate(&conn.socket(), &Socket::send);
    }

    ~ChunkedResponder()
    {
        assert(gotHeaders);
        assert(gotRequest);
        assert(gotClose);
    }

    void onHeaders(http::Request& request)
    {
        gotHeaders = true;
    }

    void onRequest(http::Request& request, http::Response& response)
    {
        gotRequest = true;

        connection().response().set("Access-Control-Allow-Origin", "*");
        connection().response().set("Content-Type", "text/html");
        connection().response().set("Transfer-Encoding", "chunked");

        // headers pushed through automatically
        //connection().sendHeader();

        // Start shooting data at the client
        dataSource.init();
        // assert(0 && "fixme");
    }

    void onClose()
    {
        DebugL << "On connection close" << endl;
        gotClose = true;
        dataSource.cancel();
    }
};


class WebSocketResponder: public http::ServerResponder
{
public:
    bool gotPayload;
    bool gotClose;

    WebSocketResponder(http::ServerConnection& conn) :
        http::ServerResponder(conn),
        gotPayload(false),
        gotClose(false)
    {
        DebugL << "Creating" << endl;
    }

    virtual ~WebSocketResponder()
    {
        DebugL << "destroy" << endl;
        assert(gotPayload);
        assert(gotClose);
    }

    virtual void onPayload(const MutableBuffer& body)
    {
        DebugL << "On payload: " << body.size() << endl;

        gotPayload = true;

        // Echo the request back to the client
        connection().send(body.cstr(), body.size());
    }

    virtual void onClose()
    {
        DebugL << "On connection close" << endl;
        gotClose = true;
    }
};


class OurServerResponderFactory: public http::ServerResponderFactory
    /// A Server Responder Factory for testing the HTTP server
{
public:
    http::ServerResponder* createResponder(http::ServerConnection& conn)
    {
        std::ostringstream os;
        conn.request().write(os);
        std::string headers(os.str().data(), os.str().length());
        DebugL << "Incoming Request: " << headers << endl; // remove me

        std::string uri(conn.request().getURI());

        if (uri == "/chunked")
            return new ChunkedResponder(conn);
        else if (uri == "/websocket")
            return new WebSocketResponder(conn);
        else if (uri == "/echo")
            return new BasicEchoResponder(conn);
        else
            return new BasicResponder(conn);
    }
};


} // } // namespace scy::http