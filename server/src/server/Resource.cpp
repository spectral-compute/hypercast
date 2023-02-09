#include "Resource.hpp"

Server::Resource::~Resource() = default;

bool Server::Resource::getAllowNonEmptyPath() const noexcept { return false; }
bool Server::Resource::getAllowGet() const noexcept { return false; }
bool Server::Resource::getAllowPost() const noexcept { return false; }
bool Server::Resource::getAllowPut() const noexcept { return false; }
