#ifndef _PASSENGER_MESSAGE_CHANNEL_H_
#define _PASSENGER_MESSAGE_CHANNEL_H_

#include <algorithm>
#include <string>
#include <list>
#include <vector>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <cstdarg>

#include "Exceptions.h"
#include "Utils.h"

namespace Passenger {

using namespace std;

/**
 * Convenience class for I/O operations on file descriptors.
 *
 * This class provides convenience methods for:
 *  - sending and receiving raw data over a file descriptor.
 *  - sending and receiving messages over a file descriptor.
 *  - file descriptor passing over a Unix socket.
 * All of these methods use exceptions for error reporting.
 *
 * There are two kinds of messages:
 *  - Array messages. These are just a list of strings, and the message
 *    itself has a specific length. The contained strings may not
 *    contain NUL characters (<tt>'\\0'</tt>).
 *  - Scalar messages. These are byte strings which may contain arbitrary
 *    binary data. Scalar messages also have a specific length.
 * The protocol is designed to be low overhead, easy to implement and
 * easy to parse.
 *
 * MessageChannel is to be wrapped around a file descriptor. For example:
 * @code
 *    int p[2];
 *    pipe(p);
 *    MessageChannel channel1(p[0]);
 *    MessageChannel channel2(p[1]);
 *    
 *    // Send an array message.
 *    channel2.write("hello", "world !!", NULL);
 *    list<string> args;
 *    channel1.read(args);    // args now contains { "hello", "world !!" }
 *
 *    // Send a scalar message.
 *    channel2.writeScalar("some long string which can contain arbitrary binary data");
 *    string str;
 *    channel1.readScalar(str);
 * @endcode
 *
 * The life time of a MessageChannel is independent from that of the
 * wrapped file descriptor. If a MessageChannel object is destroyed,
 * the file descriptor is not automatically closed. Call close()
 * if you want to close the file descriptor.
 *
 * @note I/O operations are not buffered.
 * @note Be careful with mixing the sending/receiving of array messages,
 *    scalar messages and file descriptors. If you send a collection of any
 *    of these in a specific order, then the receiving side must receive them
 *    in the exact some order. So suppose you first send a message, then a
 *    file descriptor, then a scalar, then the receiving side must first
 *    receive a message, then a file descriptor, then a scalar. If the
 *    receiving side does things in the wrong order then bad things will
 *    happen.
 * @note MessageChannel is thread-safe.
 *
 * @ingroup Support
 */
class MessageChannel {
private:
	const static char DELIMITER = '\0';
	int fd;

public:
	/**
	 * Construct a new MessageChannel with no underlying file descriptor.
	 * Thus the resulting MessageChannel object will not be usable.
	 * This constructor exists to allow one to declare an "empty"
	 * MessageChannel variable which is to be initialized later.
	 */
	MessageChannel() {
		this->fd = -1;
	}

	/**
	 * Construct a new MessageChannel with the given file descriptor.
	 */
	MessageChannel(int fd) {
		this->fd = fd;
	}
	
	/**
	 * Close the underlying file descriptor. If this method is called multiple
	 * times, the file descriptor will only be closed the first time.
	 */
	void close() {
		if (fd != -1) {
			::close(fd);
			fd = -1;
		}
	}

	/**
	 * Send an array message, which consists of the given elements, over the underlying
	 * file descriptor.
	 *
	 * @param args The message elements.
	 * @throws SystemException An error occured while writing the data to the file descriptor.
	 * @pre None of the message elements may contain a NUL character (<tt>'\\0'</tt>).
	 * @see read(), write(const char *, ...)
	 */
	void write(const list<string> &args) {
		list<string>::const_iterator it;
		string data;
		uint16_t dataSize = 0;

		for (it = args.begin(); it != args.end(); it++) {
			dataSize += it->size() + 1;
		}
		data.reserve(dataSize + sizeof(dataSize));
		dataSize = htons(dataSize);
		data.append((const char *) &dataSize, sizeof(dataSize));
		for (it = args.begin(); it != args.end(); it++) {
			data.append(*it);
			data.append(1, DELIMITER);
		}
		
		writeRaw(data);
	}
	
	/**
	 * Send an array message, which consists of the given strings, over the underlying
	 * file descriptor.
	 *
	 * @param name The first element of the message to send.
	 * @param ... Other elements of the message. These *must* be strings, i.e. of type char*.
	 *            It is also required to terminate this list with a NULL.
	 * @throws SystemException An error occured while writing the data to the file descriptor.
	 * @pre None of the message elements may contain a NUL character (<tt>'\\0'</tt>).
	 * @see read(), write(const list<string> &)
	 */
	void write(const char *name, ...) {
		list<string> args;
		args.push_back(name);
		
		va_list ap;
		va_start(ap, name);
		while (true) {
			const char *arg = va_arg(ap, const char *);
			if (arg == NULL) {
				break;
			} else {
				args.push_back(arg);
			}
		}
		va_end(ap);
		write(args);
	}
	
	/**
	 * Send a scalar message over the underlying file descriptor.
	 *
	 * @param str The scalar message's content.
	 * @throws SystemException An error occured while writing the data to the file descriptor.
	 * @see readScalar(), writeScalar(const char *, unsigned int)
	 */
	void writeScalar(const string &str) {
		writeScalar(str.c_str(), str.size());
	}
	
	/**
	 * Send a scalar message over the underlying file descriptor.
	 *
	 * @param data The scalar message's content.
	 * @param size The number of bytes in <tt>data</tt>.
	 * @pre <tt>data != NULL</tt>
	 * @throws SystemException An error occured while writing the data to the file descriptor.
	 * @see readScalar(), writeScalar(const string &)
	 */
	void writeScalar(const char *data, unsigned int size) {
		uint32_t l = htonl(size);
		writeRaw((const char *) &l, sizeof(uint32_t));
		writeRaw(data, size);
	}
	
	/**
	 * Send a block of data over the underlying file descriptor.
	 * This method blocks until everything is sent.
	 *
	 * @param data The data to send.
	 * @param size The number of bytes in <tt>data</tt>.
	 * @pre <tt>data != NULL</tt>
	 * @throws SystemException An error occured while writing the data to the file descriptor.
	 * @see readRaw()
	 */
	void writeRaw(const char *data, unsigned int size) {
		ssize_t ret;
		unsigned int written = 0;
		do {
			do {
				ret = ::write(fd, data + written, size - written);
			} while (ret == -1 && errno == EINTR);
			if (ret == -1) {
				throw SystemException("write() failed", errno);
			} else {
				written += ret;
			}
		} while (written < size);
	}
	
	/**
	 * Send a block of data over the underlying file descriptor.
	 * This method blocks until everything is sent.
	 *
	 * @param data The data to send.
	 * @pre <tt>data != NULL</tt>
	 * @throws SystemException An error occured while writing the data to the file descriptor.
	 */
	void writeRaw(const string &data) {
		writeRaw(data.c_str(), data.size());
	}
	
	/**
	 * Pass a file descriptor. This only works if the underlying file
	 * descriptor is a Unix socket.
	 *
	 * @param fileDescriptor The file descriptor to pass.
	 * @throws SystemException Something went wrong during file descriptor passing.
	 * @pre <tt>fileDescriptor >= 0</tt>
	 * @see readFileDescriptor()
	 */
	void writeFileDescriptor(int fileDescriptor) {
		struct msghdr msg;
		struct iovec vec[1];
		char buf[1];
		struct {
			struct cmsghdr hdr;
			int fd;
		} cmsg;
	
		msg.msg_name = NULL;
		msg.msg_namelen = 0;
	
		/* Linux and Solaris doesn't work if msg_iov is NULL. */
		buf[0] = '\0';
		vec[0].iov_base = buf;
		vec[0].iov_len = 1;
		msg.msg_iov = vec;
		msg.msg_iovlen = 1;
	
		msg.msg_control = (caddr_t)&cmsg;
		msg.msg_controllen = CMSG_SPACE(sizeof(int));
		msg.msg_flags = 0;
		cmsg.hdr.cmsg_len = CMSG_LEN(sizeof(int));
		cmsg.hdr.cmsg_level = SOL_SOCKET;
		cmsg.hdr.cmsg_type = SCM_RIGHTS;
		cmsg.fd = fileDescriptor;
		
		if (sendmsg(fd, &msg, 0) == -1) {
			throw SystemException("Cannot send file descriptor with sendmsg()", errno);
		}
	}
	
	/**
	 * Receive a message from the underlying file descriptor.
	 *
	 * @param args The message will be put in this variable.
	 * @return Whether end-of-file has been reached. If so, then the contents
	 *         of <tt>args</tt> will be undefined.
	 * @throws SystemException If an error occured while receiving the message.
	 * @see write()
	 */
	bool read(vector<string> &args) {
		uint16_t size;
		int ret;
		unsigned int alreadyRead = 0;
		
		do {
			do {
				ret = ::read(fd, (char *) &size + alreadyRead, sizeof(size) - alreadyRead);
			} while (ret == -1 && errno == EINTR);
			if (ret == -1) {
				throw SystemException("read() failed", errno);
			} else if (ret == 0) {
				return false;
			}
			alreadyRead += ret;
		} while (alreadyRead < sizeof(size));
		size = ntohs(size);
		
		string buffer;
		args.clear();
		buffer.reserve(size);
		while (buffer.size() < size) {
			char tmp[1024 * 8];
			do {
				ret = ::read(fd, tmp, min(size - buffer.size(), sizeof(tmp)));
			} while (ret == -1 && errno == EINTR);
			if (ret == -1) {
				throw SystemException("read() failed", errno);
			} else if (ret == 0) {
				return false;
			}
			buffer.append(tmp, ret);
		}
		
		if (!buffer.empty()) {
			string::size_type start = 0, pos;
			const string &const_buffer(buffer);
			while ((pos = const_buffer.find('\0', start)) != string::npos) {
				args.push_back(const_buffer.substr(start, pos - start));
				start = pos + 1;
			}
		}
		return true;
	}
	
	/**
	 * Read a scalar message from the underlying file descriptor.
	 *
	 * @param output The message will be put in here.
	 * @returns Whether end-of-file was reached during reading.
	 * @throws SystemException An error occured while writing the data to the file descriptor.
	 * @see writeScalar()
	 */
	bool readScalar(string &output) {
		uint32_t size;
		unsigned int remaining;
		
		if (!readRaw(&size, sizeof(uint32_t))) {
			return false;
		}
		size = ntohl(size);
		
		output.clear();
		output.reserve(size);
		remaining = size;
		while (remaining > 0) {
			char buf[1024 * 32];
			unsigned int blockSize = min(sizeof(buf), remaining);
			
			if (!readRaw(buf, blockSize)) {
				return false;
			}
			output.append(buf, blockSize);
			remaining -= blockSize;
		}
		return true;
	}
	
	/**
	 * Read exactly <tt>size</tt> bytes of data from the underlying file descriptor,
	 * and put the result in <tt>buf</tt>. If end-of-file has been reached, or if
	 * end-of-file was encountered before <tt>size</tt> bytes have been read, then
	 * <tt>false</tt> will be returned. Otherwise (i.e. if the read was successful),
	 * <tt>true</tt> will be returned.
	 *
	 * @param buf The buffer to place the read data in. This buffer must be at least
	 *            <tt>size</tt> bytes long.
	 * @param size The number of bytes to read.
	 * @return Whether reading was successful or whether EOF was reached.
	 * @pre buf != NULL
	 * @throws SystemException Something went wrong during reading.
	 * @see writeRaw()
	 */
	bool readRaw(void *buf, unsigned int size) {
		ssize_t ret;
		unsigned int alreadyRead = 0;
		
		while (alreadyRead < size) {
			do {
				ret = ::read(fd, (char *) buf + alreadyRead, size - alreadyRead);
			} while (ret == -1 && errno == EINTR);
			if (ret == -1) {
				throw SystemException("read() failed", errno);
			} else if (ret == 0) {
				return false;
			} else {
				alreadyRead += ret;
			}
		}
		return true;
	}
	
	/**
	 * Receive a file descriptor, which had been passed over the underlying
	 * file descriptor.
	 *
	 * @return The passed file descriptor.
	 * @throws SystemException If something went wrong during the
	 *            receiving of a file descriptor. Perhaps the underlying
	 *            file descriptor isn't a Unix socket.
	 * @throws IOException Whatever was received doesn't seem to be a
	 *            file descriptor.
	 */
	int readFileDescriptor() {
		struct msghdr msg;
		struct iovec vec[2];
		char buf[1];
		struct {
			struct cmsghdr hdr;
			int fd;
		} cmsg;

		msg.msg_name = NULL;
		msg.msg_namelen = 0;
	
		vec[0].iov_base = buf;
		vec[0].iov_len = sizeof(buf);
		msg.msg_iov = vec;
		msg.msg_iovlen = 1;

		msg.msg_control = (caddr_t)&cmsg;
		msg.msg_controllen = CMSG_SPACE(sizeof(int));
		msg.msg_flags = 0;
		cmsg.hdr.cmsg_len = CMSG_LEN(sizeof(int));
		cmsg.hdr.cmsg_level = SOL_SOCKET;
		cmsg.hdr.cmsg_type = SCM_RIGHTS;
		cmsg.fd = -1;

		if (recvmsg(fd, &msg, 0) == -1) {
			throw SystemException("Cannot read file descriptor with recvmsg()", errno);
		}

		if (msg.msg_controllen != CMSG_SPACE(sizeof(int))
		 || cmsg.hdr.cmsg_len != CMSG_SPACE(0) + sizeof(int)
		 || cmsg.hdr.cmsg_level != SOL_SOCKET
		 || cmsg.hdr.cmsg_type != SCM_RIGHTS) {
			throw IOException("No valid file descriptor received.");
		}
		return cmsg.fd;
	}
};

} // namespace Passenger

#endif /* _PASSENGER_MESSAGE_CHANNEL_H_ */
