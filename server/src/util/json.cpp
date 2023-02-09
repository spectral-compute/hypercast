#include "json.hpp"

using namespace std::string_literals;

Json::ObjectDeserializer::Exception::~Exception() = default;
Json::ObjectDeserializer::Exception::Exception(const char *key, std::string_view message) :
    std::runtime_error(key ? "Error parsing JSON object at key \""s + key + "\": " + std::string(message) :
                             ("Error parsing JSON object: " + std::string(message))),
    key(key ? std::optional(std::string(key)) : std::nullopt), message(message)
{
}

Json::ObjectDeserializer::~ObjectDeserializer() = default;

Json::ObjectDeserializer::ObjectDeserializer(const nlohmann::json &j, const char *parent) : parent(parent), j(j)
{
    /* Make sure we're dealing with an object. */
    if (!j.is_object()) {
        throw Exception(parent, "Value is not an object.");
    }
}

void Json::ObjectDeserializer::operator()() const
{
    for (const auto &[key, value]: j.items()) {
        if (!validKeys.contains(key)) {
            throw Exception(parent, "Subkey \""s + key + "\" is unknown.");
        }
    }
}

nlohmann::json::const_iterator Json::ObjectDeserializer::getIteratorForKey(const char *key, bool required)
{
    /* Record that we've asked for this key so the destructor doesn't generate an error about it. */
    validKeys.insert(key);

    /* Try to find the key. */
    auto it = j.find(key);
    if (required && it == j.end()) {
        throw Exception(parent, "Subkey \""s + key + "\" not found.");
    }

    /* Done :) */
    return it;
}

void Json::ObjectDeserializer::convertWithLambda(const char *key, bool required,
                                                 const std::function<void (const nlohmann::json &)> &fn)
{
    /* Try to extract the value. */
    auto it = getIteratorForKey(key, required);
    if (it == j.end()) {
        return;
    }

    /* If we found the key, convert the object to the destination type. */
    try {
        fn(*it);
    }
    catch (const nlohmann::json::type_error &e) {
        throw Exception(parent, "Subkey \""s + key + "\" value has incorrect type: " + e.what() + ".");
    }
    catch (const nlohmann::json::out_of_range &e) {
        throw Exception(parent, "Subkey \""s + key + "\" value is out of range: " + e.what() + ".");
    }
}

std::optional<int> Json::ObjectDeserializer::convertWithIntNameMap(const char *key, bool required,
                                                                   const std::initializer_list<IntName> &values)
{
    /* Try to extract the value. */
    auto it = getIteratorForKey(key, required);
    if (it == j.end()) {
        return std::nullopt;
    }

    /* We must have a string. */
    if (!it->is_string()) {
        throw Exception(parent, "Subkey \""s + key + "\" value is not a string.");
    }
    std::string string = it->get<std::string>();

    /* Figure out which value it is. */
    for (auto [value, name]: values) {
        if (string == name) {
            return value;
        }
    }

    /* Otherwise, the value is unknown. */
    std::string possibleValues;
    for (auto [value, name]: values) {
        possibleValues += ", \""s + name + "\"";
    }
    possibleValues = possibleValues.substr(2);
    throw Exception(parent, "Subkey \""s + key + "\" value is \"" + string + "\", not any of: " + possibleValues + ".");
}

nlohmann::json Json::parse(std::string_view jsonString, bool allowComments)
{
    return nlohmann::json::parse(jsonString, nullptr, true, allowComments);
}

std::string Json::dump(const nlohmann::json &json, int indent)
{
    return json.dump(indent);
}
