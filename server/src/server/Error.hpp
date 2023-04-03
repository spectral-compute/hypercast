#pragma once

#include <string>
#include <string_view>

namespace Server
{

/**
 * Response errors.
 *
 * These errors correspond to HTTP response codes. Note that the HTTP implementation might return other errors to the
 * client.
 */
enum class ErrorKind
{
    /**
     * Malformed request for the resource.
     *
     * Equivalent to HTTP 400 Bad Request.
     */
    BadRequest,

    /**
     * The client is not allowed to access the resource.
     *
     * Equivalent to HTTP 403 Forbidden.
     */
    Forbidden,

    /**
     * The resource does not exist.
     *
     * Equivalent to HTTP 404 Not Found.
     */
    NotFound,

    /**
     * Unsupported request type (Request::Type).
     *
     * Equivalent to HTTP 405 Method Not Allowed.
     */
    UnsupportedType,

    /**
     * The target resource is in such a state that the request cannot be processed.
     *
     * Equivalent to HTTP 409 Conflict.
     */
    Conflict,

    /**
     * An unknown or internal error happened.
     *
     * Equivalent to HTTP 500 Internal Server Error.
     */
    Internal
};

/**
 * An object that can be thrown as an exception from Resource::operator().
 *
 * If thrown by Resource::operator() before in Response::operator<< is called, Response::setErrorAndMessage is called
 * with its arguments set to values from this object.
 */
struct Error final
{
    Error(ErrorKind kind, std::string_view message = {}) : kind(kind), message(message) {}
    Error(ErrorKind kind, const char *message) : kind(kind), message(message) {}
    Error(ErrorKind kind, std::string message) : kind(kind), message(std::move(message)) {}

    ErrorKind kind;
    std::string message;
};

} // namespace Server
