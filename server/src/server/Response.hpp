#pragma once

#include "CacheKind.hpp"
#include "Error.hpp"

#include <cassert>
#include <optional>
#include <span>
#include <vector>
#include <map>
#include <boost/beast/http/field.hpp>

template <typename> class Awaitable;

namespace Server
{

/**
 * A response to a request for a particular resource.
 *
 * The response consists of two parts:
 * 1. Headers (e.g: error, cache kind, or mime type).
 * 2. Body data.
 *
 * The headers are guaranteed not to be sent until after one of the body write methods (operator<<) is called, wait is
 * called, or the Request's operator() returns. After this, the header setting methods must not be called. The headers
 * can be sent even before any data is available by writing empty data.
 *
 * This is subclassed by the implementation of the HTTP server to provide an implementation that's suitable for the
 * asynchronous networking stuff the HTTP server does.
 */
class Response
{
public:
    virtual ~Response();

    // It makes no sense to copy a Response object.
    Response(const Response &) = delete;
    Response &operator=(const Response &) = delete;

    /**
     * Determine whether any of the write methods have been called.
     */
    bool getWriteStarted() const
    {
        return writeStarted;
    }

    /**
     * Set that this response is an error response.
     *
     * If this method is called, it must be called before operator<<.
     */
    void setError(ErrorKind kind)
    {
        assert(!getWriteStarted());
        errorKind = kind;
    }

    /**
     * Set an error, and write a message.
     *
     * Unlike setError(), this also sets the MIME type appropriate to the message.
     *
     * If this method is called, it must be called before operator<< and wait. No other non-const method of this class
     * may be called after this method.
     *
     * @param kind The error kind.
     * @param message The message to include.
     */
    void setErrorAndMessage(ErrorKind kind, std::string_view message = {});

    /**
     * Set the cache kind.
     *
     * The default if this method is not called is CacheKind::fixed. If this method is called, it must be called before
     * operator<< and wait.
     */
    void setCacheKind(CacheKind kind)
    {
        assert(!getWriteStarted());
        cacheKind = kind;
    }

    /**
     * Set the MIME type.
     *
     * The default if this method is not called is no MIME type. If this method is called, it must be called before
     * operator<< and wait.
     */
    void setMimeType(std::string type)
    {
        assert(!getWriteStarted());
        mimeType = std::move(type);
    }

    /**
     * Write (by appending) data to the response body.
     *
     * @see wait.
     *
     * @param data The data to write.
     */
    Response &operator<<(std::vector<std::byte> data)
    {
        writeBody(std::move(data));
        writeStarted = true; // We've now started writing. This is after writeBody() so it can detect the first write.
        return *this;
    }

    /**
     * Write (by appending) data to the response body.
     *
     * @see wait.
     *
     * @param data The data to write.
     */
    Response &operator<<(std::span<const std::byte> data)
    {
        return (*this) << std::vector<std::byte>(data.begin(), data.end());
    }

    /**
     * Write (by appending) a string to the response body.
     *
     * @see wait.
     *
     * @param string The string to write.
     */
    Response &operator<<(std::string_view string)
    {
        return (*this) << std::span((const std::byte *)string.data(), string.size());
    }

    /**
     * Wait for outstanding response body data to be written, at least down to some "low water line" buffer level.
     *
     * It's not guaranteed that any data will be written at all until this is called, however this is always called once
     * a resource's operator() returns. This is generally only important if there might be a significant amount of time
     * between operator<< being called and the resource's operator() returning.
     *
     * This method is also useful to avoid buffering a large amount of data in case the response is large.
     *
     * @param end If set to true, no more data can be output to the response body. Resource::operator() must not call
     *            this with true, because Server::operator() does so.
     */
    Awaitable<void> flush(bool end = false);

    /**
     * Set an HTTP response header.
     *
     * Why does this class even exist. Half of it is randomly split out into HttpResponse.
     */
    void setHeader(const boost::beast::http::field& name, const std::string& value) {
        extraHeaders[name] = value;
    }

protected:
    Response() = default;

    /**
     * Get the error kind, if any.
     *
     * This should not be called until after the first call to writeBody.
     */
    std::optional<ErrorKind> getErrorKind() const
    {
        return errorKind;
    }

    /**
     * Get the cache kind.
     *
     * This should not be called until after the first call to writeBody.
     */
    CacheKind getCacheKind() const
    {
        return cacheKind;
    }

    /**
     * Get the MIME type kind.
     *
     * This should not be called until after the first call to writeBody.
     */
    const std::string &getMimeType() const
    {
        return mimeType;
    }

private:
    /**
     * Write some data to the response body.
     *
     * @param data The data to write to the response body.
     */
    virtual void writeBody(std::vector<std::byte> data) = 0;

    /**
     * Implementation for wait.
     *
     * The name works because the body (technically with newlines after) is the last thing to be transmitted.
     */
    virtual Awaitable<void> flushBody(bool end) = 0;

    std::optional<ErrorKind> errorKind;
    CacheKind cacheKind = CacheKind::fixed;
    std::string mimeType;
    bool writeStarted = false;

protected:
    // Custom response headers the resource has decided it wants to send.
    std::map<boost::beast::http::field, std::string> extraHeaders;
};

} // namespace Server
