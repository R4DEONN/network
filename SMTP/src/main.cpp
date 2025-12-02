#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

struct SslContext
{
	SSL_CTX* ctx;

	SslContext() : ctx(nullptr)
	{
	}

	~SslContext()
	{
		if (ctx) SSL_CTX_free(ctx);
	}
};

struct SslConnection
{
	SSL* ssl;

	SslConnection() : ssl(nullptr)
	{
	}

	~SslConnection()
	{
		if (ssl) SSL_free(ssl);
	}
};

struct Socket
{
	int fd;

	Socket() : fd(-1)
	{
	}

	~Socket()
	{
		if (fd >= 0) close(fd);
	}

	operator int() const
	{
		return fd;
	}
};

std::string base64_encode(const std::string& input)
{
	BIO* b64, * bmem;
	b64 = BIO_new(BIO_f_base64());
	bmem = BIO_new(BIO_s_mem());
	b64 = BIO_push(b64, bmem);
	BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
	BIO_write(b64, input.c_str(), input.size());
	BIO_flush(b64);
	BUF_MEM* bptr;
	BIO_get_mem_ptr(b64, &bptr);
	std::string result(bptr->data, bptr->length);
	BIO_free_all(b64);
	return result;
}

std::string read_line(int sock)
{
	std::string line;
	char ch;
	while (true)
	{
		ssize_t bytes = read(sock, &ch, 1);
		if (bytes <= 0)
		{
			if (errno == EWOULDBLOCK || errno == EAGAIN)
			{
				std::cerr << "⚠️ Read timeout\n";
			}
			break;
		}
		if (ch == '\n') break;
		if (ch != '\r') line += ch;
	}
	std::cout << "[DEBUG] Raw line: [" << line << "]\n";
	return line;
}

std::string read_ssl_line(SSL* ssl)
{
	std::string line;
	char ch;
	while (true)
	{
		int bytes = SSL_read(ssl, &ch, 1);
		if (bytes <= 0)
		{
			int err = SSL_get_error(ssl, bytes);
			if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_ZERO_RETURN)
			{
				unsigned long e = ERR_get_error();
				if (e != 0)
				{
					char err_buf[256];
					ERR_error_string_n(e, err_buf, sizeof(err_buf));
					std::cerr << "SSL_read error: " << err_buf << "\n";
				}
			}
			break;
		}
		if (ch == '\n') break;
		if (ch != '\r') line += ch;
	}
	std::cout << "[DEBUG] SSL line: [" << line << "]\n";
	return line;
}

bool send_ehlo(int sock, const std::string& domain)
{
	std::string cmd = "EHLO " + domain + "\r\n";
	write(sock, cmd.c_str(), cmd.length());
	std::cout << "C: " << cmd;

	while (true)
	{
		std::string line = read_line(sock);
		if (line.empty()) return false;
		if (line.size() >= 4 && line.substr(0, 4) == "250 ") return true;
		if (line.size() >= 4 && line.substr(0, 4) == "250-") continue;
		return false;
	}
}

bool send_ehlo_ssl(SSL* ssl, const std::string& domain)
{
	std::string cmd = "EHLO " + domain + "\r\n";
	SSL_write(ssl, cmd.c_str(), cmd.length());
	std::cout << "C: " << cmd;

	while (true)
	{
		std::string line = read_ssl_line(ssl);
		if (line.empty()) return false;
		if (line.size() >= 4 && line.substr(0, 4) == "250 ") return true;
		if (line.size() >= 4 && line.substr(0, 4) == "250-") continue;
		return false;
	}
}

bool send_command(int sock, const std::string& cmd, const std::string& expected_code)
{
	write(sock, cmd.c_str(), cmd.length());
	std::cout << "C: " << cmd;
	std::string resp = read_line(sock);
	return resp.size() >= expected_code.size() && resp.substr(0, expected_code.size()) == expected_code;
}

bool send_ssl_command(SSL* ssl, const std::string& cmd, const std::string& expected_code)
{
	SSL_write(ssl, cmd.c_str(), cmd.length());
	std::cout << "C: " << cmd;
	std::string resp = read_ssl_line(ssl);
	return resp.size() >= expected_code.size() && resp.substr(0, expected_code.size()) == expected_code;
}

std::string create_mime_message(
	const std::string& from,
	const std::string& to,
	const std::string& subject,
	const std::string& body,
	const std::vector<std::string>& attachment_paths
)
{
	const std::string boundary = "----=_NextPart_SMTP_CPP_000_0000";
	std::string message;

	message += "From: " + from + "\r\n";
	message += "To: " + to + "\r\n";
	message += "Subject: " + subject + "\r\n";
	message += "MIME-Version: 1.0\r\n";
	message += "Content-Type: multipart/mixed; boundary=\"" + boundary + "\"\r\n";
	message += "\r\n";

	message += "--" + boundary + "\r\n";
	message += "Content-Type: text/plain; charset=UTF-8\r\n";
	message += "\r\n";
	message += body + "\r\n";

	for (const auto& path: attachment_paths)
	{
		std::ifstream file(path, std::ios::binary);
		if (!file)
		{
			std::cerr << "Cannot open attachment: " << path << std::endl;
			continue;
		}
		std::string content((std::istreambuf_iterator<char>(file)),
			std::istreambuf_iterator<char>());
		file.close();

		size_t slash = path.find_last_of("/\\");
		std::string filename = (slash == std::string::npos) ? path : path.substr(slash + 1);

		std::string mime_type = "application/octet-stream";
		if (filename.size() >= 4)
		{
			std::string ext = filename.substr(filename.size() - 4);
			if (ext == ".txt")
			{
				mime_type = "text/plain; charset=UTF-8";
			}
			else if (ext == ".png")
			{
				mime_type = "image/png";
			}
			else if (ext == ".jpg")
			{
				mime_type = "image/jpeg";
			}
			else if (filename.size() >= 5 && filename.substr(filename.size() - 5) == ".jpeg")
			{
				mime_type = "image/jpeg";
			}
			else if (ext == ".pdf")
			{
				mime_type = "application/pdf";
			}
		}

		message += "--" + boundary + "\r\n";
		message += "Content-Type: " + mime_type + "\r\n";
		message += "Content-Transfer-Encoding: base64\r\n";
		message += "Content-Disposition: attachment; filename=\"" + filename + "\"\r\n";
		message += "\r\n";

		std::string encoded = base64_encode(content);
		for (size_t i = 0; i < encoded.size(); i += 76)
		{
			message += encoded.substr(i, 76) + "\r\n";
		}
	}

	message += "--" + boundary + "--\r\n";
	return message;
}

struct Args
{
	std::string host = "smtp.gmail.com";
	std::string port = "587";
	std::string from;
	std::string to;
	std::string subject;
	std::string body = "";
	std::string user;
	std::string pass;
	std::vector<std::string> attachments;
};

Args parse_args(int argc, char* argv[])
{
	Args args;
	for (int i = 1; i < argc; ++i)
	{
		std::string arg = argv[i];
		if (arg == "--host" && i + 1 < argc) args.host = argv[++i];
		else if (arg == "--port" && i + 1 < argc) args.port = argv[++i];
		else if (arg == "--from" && i + 1 < argc) args.from = argv[++i];
		else if (arg == "--to" && i + 1 < argc) args.to = argv[++i];
		else if (arg == "--subject" && i + 1 < argc) args.subject = argv[++i];
		else if (arg == "--body" && i + 1 < argc) args.body = argv[++i];
		else if (arg == "--user" && i + 1 < argc) args.user = argv[++i];
		else if (arg == "--pass" && i + 1 < argc) args.pass = argv[++i];
		else if (arg == "--attach" && i + 1 < argc) args.attachments.push_back(argv[++i]);
		else
		{
			std::cerr << "Unknown argument: " << arg << std::endl;
			exit(1);
		}
	}
	if (args.from.empty() || args.to.empty() || args.user.empty() || args.pass.empty())
	{
		std::cerr << "Missing required: --from, --to, --user, --pass\n";
		exit(1);
	}
	return args;
}

int main(int argc, char* argv[])
{
	Args args = parse_args(argc, argv);

	Socket sock;
	sock.fd = socket(AF_INET, SOCK_STREAM, 0);
	if (sock.fd == -1)
	{
		std::cerr << "Socket error: " << strerror(errno) << std::endl;
		return 1;
	}

	struct timeval tv;
	tv.tv_sec = 10;
	tv.tv_usec = 0;
	setsockopt(sock.fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

	struct addrinfo hints = {}, * res;
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	if (getaddrinfo(args.host.c_str(), args.port.c_str(), &hints, &res) != 0)
	{
		std::cerr << "getaddrinfo failed\n";
		return 1;
	}

	if (connect(sock.fd, res->ai_addr, res->ai_addrlen) == -1)
	{
		std::cerr << "Connect failed: " << strerror(errno) << std::endl;
		freeaddrinfo(res);
		return 1;
	}
	freeaddrinfo(res);
	std::cout << "✅ Connected to " << args.host << ":" << args.port << std::endl;

	std::string greeting = read_line(sock.fd);
	if (greeting.substr(0, 3) != "220")
	{
		std::cerr << "Bad greeting: " << greeting << std::endl;
		return 1;
	}

	if (!send_ehlo(sock.fd, args.host))
	{
		std::cerr << "EHLO failed\n";
		return 1;
	}

	if (!send_command(sock.fd, "STARTTLS\r\n", "220"))
	{
		std::cerr << "STARTTLS not accepted\n";
		return 1;
	}
	std::cout << "✅ STARTTLS accepted\n";

	SSL_library_init();
	SSL_load_error_strings();
	OpenSSL_add_ssl_algorithms();

	SslContext ssl_ctx;
	ssl_ctx.ctx = SSL_CTX_new(TLS_client_method());
	if (!ssl_ctx.ctx)
	{
		std::cerr << "SSL_CTX_new failed\n";
		return 1;
	}
	SSL_CTX_set_options(ssl_ctx.ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1);
	SSL_CTX_set_default_verify_paths(ssl_ctx.ctx);

	SslConnection ssl_conn;
	ssl_conn.ssl = SSL_new(ssl_ctx.ctx);
	if (!ssl_conn.ssl || SSL_set_fd(ssl_conn.ssl, sock.fd) != 1)
	{
		std::cerr << "SSL setup failed\n";
		return 1;
	}

	SSL_set_tlsext_host_name(ssl_conn.ssl, args.host.c_str());
	SSL_set_verify(ssl_conn.ssl, SSL_VERIFY_NONE, nullptr);

	std::cout << "🔄 SSL handshake...\n";
	if (SSL_connect(ssl_conn.ssl) != 1)
	{
		unsigned long err = ERR_get_error();
		char err_buf[256];
		ERR_error_string_n(err, err_buf, sizeof(err_buf));
		std::cerr << "SSL_connect failed: " << err_buf << std::endl;
		return 1;
	}
	std::cout << "✅ SSL connected. Cipher: " << SSL_get_cipher_name(ssl_conn.ssl) << std::endl;

	if (!send_ehlo_ssl(ssl_conn.ssl, args.host))
	{
		std::cerr << "EHLO over SSL failed\n";
		return 1;
	}

	if (!send_ssl_command(ssl_conn.ssl, "AUTH LOGIN\r\n", "334") ||
		!send_ssl_command(ssl_conn.ssl, base64_encode(args.user) + "\r\n", "334") ||
		!send_ssl_command(ssl_conn.ssl, base64_encode(args.pass) + "\r\n", "235"))
	{
		std::cerr << "Authentication failed\n";
		return 1;
	}
	std::cout << "✅ Authenticated\n";

	if (!send_ssl_command(ssl_conn.ssl, "MAIL FROM:<" + args.from + ">\r\n", "250") ||
		!send_ssl_command(ssl_conn.ssl, "RCPT TO:<" + args.to + ">\r\n", "250") ||
		!send_ssl_command(ssl_conn.ssl, "DATA\r\n", "354"))
	{
		std::cerr << "Failed during MAIL/RCPT/DATA\n";
		return 1;
	}

	std::string mime_msg = create_mime_message(args.from, args.to, args.subject, args.body, args.attachments);
	SSL_write(ssl_conn.ssl, mime_msg.c_str(), mime_msg.size());
	std::cout << "C: [MIME message sent]\n";

	if (!send_ssl_command(ssl_conn.ssl, "\r\n.\r\n", "250"))
	{
		std::cerr << "Failed to finalize message\n";
		return 1;
	}

	send_ssl_command(ssl_conn.ssl, "QUIT\r\n", "221");

	std::cout << "✅ Email sent successfully!\n";
	return 0;
}