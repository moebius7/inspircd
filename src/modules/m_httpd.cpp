/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include <algorithm>
#include "modules.h"
#include "httpd.h"

/* $ModDesc: Provides HTTP serving facilities to modules */

class ModuleHttpServer;

static ModuleHttpServer* HttpModule;
static bool claimed;

/** HTTP socket states
 */
enum HttpState
{
	HTTP_LISTEN = 0,
	HTTP_SERVE_WAIT_REQUEST = 1,
	HTTP_SERVE_RECV_POSTDATA = 2,
	HTTP_SERVE_SEND_DATA = 3
};

/** A socket used for HTTP transport
 */
class HttpServerSocket : public InspSocket
{
	FileReader* index;
	HttpState InternalState;
	std::stringstream headers;
	std::string postdata;
	std::string request_type;
	std::string uri;
	std::string http_version;
	unsigned int postsize;
	
 public:

	HttpServerSocket(InspIRCd* SI, std::string host, int port, bool listening, unsigned long maxtime, FileReader* index_page) : InspSocket(SI, host, port, listening, maxtime), index(index_page), postsize(0)
	{
		InternalState = HTTP_LISTEN;
	}

	HttpServerSocket(InspIRCd* SI, int newfd, char* ip, FileReader* ind) : InspSocket(SI, newfd, ip), index(ind), postsize(0)
	{
		InternalState = HTTP_SERVE_WAIT_REQUEST;
	}

	FileReader* GetIndex()
	{
		return index;
	}

	~HttpServerSocket()
	{
	}

	virtual int OnIncomingConnection(int newsock, char* ip)
	{
		if (InternalState == HTTP_LISTEN)
		{
			new HttpServerSocket(this->Instance, newsock, ip, index);
		}
		return true;
	}

	virtual void OnClose()
	{
	}

	std::string Response(int response)
	{
		switch (response)
		{
			case 100:
				return "CONTINUE";
			case 101:
				return "SWITCHING PROTOCOLS";
			case 200:
				return "OK";
			case 201:
				return "CREATED";
			case 202:
				return "ACCEPTED";
			case 203:
				return "NON-AUTHORITATIVE INFORMATION";
			case 204:
				return "NO CONTENT";
			case 205:
				return "RESET CONTENT";
			case 206:
				return "PARTIAL CONTENT";
			case 300:
				return "MULTIPLE CHOICES";
			case 301:
				return "MOVED PERMENANTLY";
			case 302:
				return "FOUND";
			case 303:
				return "SEE OTHER";
			case 304:
				return "NOT MODIFIED";
			case 305:
				return "USE PROXY";
			case 307:
				return "TEMPORARY REDIRECT";
			case 400:
				return "BAD REQUEST";
			case 401:
				return "UNAUTHORIZED";
			case 402:
				return "PAYMENT REQUIRED";
			case 403:
				return "FORBIDDEN";
			case 404:
				return "NOT FOUND";
			case 405:
				return "METHOD NOT ALLOWED";
			case 406:
				return "NOT ACCEPTABLE";
			case 407:
				return "PROXY AUTHENTICATION REQUIRED";
			case 408:
				return "REQUEST TIMEOUT";
			case 409:
				return "CONFLICT";
			case 410:
				return "GONE";
			case 411:
				return "LENGTH REQUIRED";
			case 412:
				return "PRECONDITION FAILED";
			case 413:
				return "REQUEST ENTITY TOO LARGE";
			case 414:
				return "REQUEST-URI TOO LONG";
			case 415:
				return "UNSUPPORTED MEDIA TYPE";
			case 416:
				return "REQUESTED RANGE NOT SATISFIABLE";
			case 417:
				return "EXPECTATION FAILED";
			case 500:
				return "INTERNAL SERVER ERROR";
			case 501:
				return "NOT IMPLEMENTED";
			case 502:
				return "BAD GATEWAY";
			case 503:
				return "SERVICE UNAVAILABLE";
			case 504:
				return "GATEWAY TIMEOUT";
			case 505:
				return "HTTP VERSION NOT SUPPORTED";
			default:
				return "WTF";
			break;
				
		}
	}

	void SendHeaders(unsigned long size, int response, const std::string &extraheaders)
	{
		time_t local = this->Instance->Time();
		struct tm *timeinfo = gmtime(&local);
		this->Write("HTTP/1.1 "+ConvToStr(response)+" "+Response(response)+"\r\nDate: ");
		this->Write(asctime(timeinfo));
		if (extraheaders.empty())
		{
			this->Write("Content-Type: text/html\r\n");
		}
		else
		{
			this->Write(extraheaders);
		}
		this->Write("Server: InspIRCd/m_httpd.so/1.1\r\nContent-Length: "+ConvToStr(size)+
				"\r\nConnection: close\r\n\r\n");
	}

	virtual bool OnDataReady()
	{
		char* data = this->Read();

		/* Check that the data read is a valid pointer and it has some content */
		if (data && *data)
		{
			headers << data;

			if (headers.str().find("\r\n\r\n") != std::string::npos)
			{
				if (request_type.empty())
				{
					headers >> request_type;
					headers >> uri;
					headers >> http_version;

					std::transform(request_type.begin(), request_type.end(), request_type.begin(), ::toupper);
					std::transform(http_version.begin(), http_version.end(), http_version.begin(), ::toupper);
				}

				if ((InternalState == HTTP_SERVE_WAIT_REQUEST) && (request_type == "POST"))
				{
					/* Do we need to fetch postdata? */
					postdata.clear();
					InternalState = HTTP_SERVE_RECV_POSTDATA;
					std::string header_item;
					while (headers >> header_item)
					{
						if (header_item == "Content-Length:")
						{
							headers >> header_item;
							postsize = atoi(header_item.c_str());
						}
					}
					if (!postsize)
					{
						InternalState = HTTP_SERVE_SEND_DATA;
						SendHeaders(0, 400, "");
						Instance->SE->WantWrite(this);
					}
					else
					{
						std::string::size_type x = headers.str().find("\r\n\r\n");
						postdata = headers.str().substr(x+4, headers.str().length());
						/* Get content length and store */
						if (postdata.length() >= postsize)
							ServeData();
					}
				}
				else if (InternalState == HTTP_SERVE_RECV_POSTDATA)
				{
					/* Add postdata, once we have it all, send the event */
					postdata.append(data);
					if (postdata.length() >= postsize)
						ServeData();
				}
				else
				{
					ServeData();
				}
				return true;
			}
			return true;
		}
		else
		{
			return false;
		}
	}

	void ServeData()
	{
		/* Headers are complete */
		InternalState = HTTP_SERVE_SEND_DATA;
	
		if ((http_version != "HTTP/1.1") && (http_version != "HTTP/1.0"))
		{
			SendHeaders(0, 505, "");
			Instance->SE->WantWrite(this);
		}
		else
		{
			if ((request_type == "GET") && (uri == "/"))
			{
				SendHeaders(index->ContentSize(), 200, "");
				this->Write(index->Contents());
				Instance->SE->WantWrite(this);
			}
			else
			{
				claimed = false;
				HTTPRequest httpr(request_type,uri,&headers,this,this->GetIP(),postdata);
				Event e((char*)&httpr, (Module*)HttpModule, "httpd_url");
				e.Send(this->Instance);
				if (!claimed)
				{
					SendHeaders(0, 404, "");
					Instance->SE->WantWrite(this);
				}
			}
		}
	}

	void Page(std::stringstream* n, int response, std::string& extraheaders)
	{
		SendHeaders(n->str().length(), response, extraheaders);
		this->Write(n->str());
		Instance->SE->WantWrite(this);
	}

	bool OnWriteReady()
	{
		return false;
	}
};

class ModuleHttpServer : public Module
{
	std::vector<HttpServerSocket*> httpsocks;
 public:

	void ReadConfig()
	{
		int port;
		std::string host;
		std::string bindip;
		std::string indexfile;
		FileReader* index;
		HttpServerSocket* http;
		ConfigReader c(ServerInstance);

		httpsocks.clear();

		for (int i = 0; i < c.Enumerate("http"); i++)
		{
			host = c.ReadValue("http", "host", i);
			bindip = c.ReadValue("http", "ip", i);
			port = c.ReadInteger("http", "port", i, true);
			indexfile = c.ReadValue("http", "index", i);
			index = new FileReader(ServerInstance, indexfile);
			if (!index->Exists())
				throw ModuleException("Can't read index file: "+indexfile);
			http = new HttpServerSocket(ServerInstance, bindip, port, true, 0, index);
			httpsocks.push_back(http);
		}
	}

	ModuleHttpServer(InspIRCd* Me) : Module(Me)
	{
		ReadConfig();
		HttpModule = this;
	}

	void OnEvent(Event* event)
	{
	}

	char* OnRequest(Request* request)
	{
		claimed = true;
		HTTPDocument* doc = (HTTPDocument*)request->GetData();
		HttpServerSocket* sock = (HttpServerSocket*)doc->sock;
		sock->Page(doc->GetDocument(), doc->GetResponseCode(), doc->GetExtraHeaders());
		return NULL;
	}

	void Implements(char* List)
	{
		List[I_OnEvent] = List[I_OnRequest] = 1;
	}

	virtual ~ModuleHttpServer()
	{
		for (size_t i = 0; i < httpsocks.size(); i++)
		{
			ServerInstance->SE->DelFd(httpsocks[i]);
			httpsocks[i]->Close();
			delete httpsocks[i]->GetIndex();
		}
		ServerInstance->InspSocketCull();
	}

	virtual Version GetVersion()
	{
		return Version(1,1,0,0,VF_VENDOR|VF_SERVICEPROVIDER,API_VERSION);
	}
};

MODULE_INIT(ModuleHttpServer)
