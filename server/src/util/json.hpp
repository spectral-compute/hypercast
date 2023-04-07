#pragma once

#include <nlohmann/json.hpp>

#include <optional>
#include <set>
#include <stdexcept>
#include <type_traits>

/// @addtogroup util
/// @{

/**
 * Namespace for JSON stuff.
 */
namespace Json
{

/**
 * A tool for deserializing a JSON object.
 *
 * This keeps track of the keys that are asked for, and throws an exception if a key exists that wasn't asked for.
 */
class ObjectDeserializer final
{
private:
    /**
     * An object for converting enums to integers.
     *
     * Using this gives a unified type to give to convertWithIntNameMap, and thus generates much less redundant code for
     * conversion of enums.
     */
    struct IntName
    {
        template <typename T> requires(std::is_enum_v<T>)
        IntName(T value, const char *name) : value((int)value), name(name) {}

        int value;
        const char *name;
    };

public:
    /**
     * An exception object that's thrown if deserialization failed.
     */
    class Exception final : public std::runtime_error
    {
    public:
        ~Exception() override;

        /**
         * Get the parent that was given to the deserializer.
         */
        const std::optional<std::string> &getKey() const
        {
            return key;
        }

        /**
         * Get an explanation of the exception.
         */
        const std::string &getMessage() const
        {
            return message;
        }

    private:
        friend class ObjectDeserializer;
        explicit Exception(const char *key, std::string_view message);

        const std::optional<std::string> key;
        const std::string message;
    };

    ~ObjectDeserializer();

    /**
     * Check to make sure a JSON value is an object with only valid keys, and throw an exception if not.
     *
     * @param j The JSON value to check.
     * @param parent The name of the field we're trying to initialize.
     */
    ObjectDeserializer(const nlohmann::json &j, const char *parent = nullptr);

    /**
     * Initialize a value from a JSON object, leaving it unchanged if it does not exist.
     *
     * @param dst The object to initialize.
     * @param key The name of the value in j.
     * @param required Throw an exception if the key does not exist.
     */
    template <typename T>
    void operator()(T &dst, const char *key, bool required = false)
    {
        convertWithLambda(key, required, [&dst](const nlohmann::json &j2) {
            dst = j2.get<typename UnwrapOptional<T>::type>();
        });
    }

    /**
     * Initialize an enum value from a JSON string, leaving it unchanged if it does not exist.
     *
     * @param dst The object to initialize.
     * @param key The name of the value in j.
     * @param required Throw an exception if the key does not exist.
     * @param values A mapping between string and corresponding enum value.
     */
    template <typename T> requires(std::is_enum_v<T>)
    void operator()(T &dst, const char *key, bool required, const std::initializer_list<IntName> &values)
    {
        std::optional<int> result = convertWithIntNameMap(key, required, values);
        if (result) {
            dst = (T)*result;
        }
    }
    template <typename T> requires(std::is_enum_v<T>)
    void operator()(T &dst, const char *key, const std::initializer_list<IntName> &values)
    {
        (*this)(dst, key, false, values);
    }
    template <typename T> requires(std::is_enum_v<T>)
    void operator()(std::optional<T> &dst, const char *key, bool required, const std::initializer_list<IntName> &values)
    {
        std::optional<int> result = convertWithIntNameMap(key, required, values);
        if (result) {
            dst = (T)*result;
        }
    }
    template <typename T> requires(std::is_enum_v<T>)
    void operator()(std::optional<T> &dst, const char *key, const std::initializer_list<IntName> &values)
    {
        (*this)(dst, key, false, values);
    }

    /**
     * Check the elements of the JSON object to make sure every element is in the set of valid keys.
     */
    void operator()() const;

private:
    /**
     * Get the JSON value's iterator for a given key directly.
     *
     * @param key The name of the value in j.
     * @param required Throw an exception if the key does not exist.
     * @return The JSON value if it exists, and std::nullopt otherwise.
     */
    nlohmann::json::const_iterator getIteratorForKey(const char *key, bool required);

    /**
     * Convert from a JSON value to a C++ value using a lambda.
     *
     * @param key The name of the value in j.
     * @param required Throw an exception if the key does not exist.
     * @param fn The function to convert from JSON value to C++ value.
     */
    void convertWithLambda(const char *key, bool required, const std::function<void (const nlohmann::json &)> &fn);

    /**
     * Converts from a string and a map between integer and string to the corresponding integer.
     *
     * @param key The name of the value in j.
     * @param required Throw an exception if the key does not exist.
     * @param values A mapping between string and corresponding enum value.
     * @return The integer value forresponding to the string if converted, and std::nullopt if the key does not exist.
     */
    std::optional<int> convertWithIntNameMap(const char *key, bool required,
                                             const std::initializer_list<IntName> &values);

    /**
     * Remove std::optional from a type.
     */
    template <typename T> struct UnwrapOptional final { using type = T; };
    template <typename T> struct UnwrapOptional<std::optional<T>> final { using type = T; };

    const char *parent;
    const nlohmann::json &j;
    std::set<std::string> validKeys;
};

/**
 * Parse a JSON string.
 *
 * It's also a little more convenient than nlohmann::json::parse because it doesn't expose arguments we don't care
 * about.
 *
 * This also avoids code-generating an entire JSON parser repeatedly.
 *
 * @param jsonString The string representation of a JSON object.
 * @param allowComments Whether or not to allow comments.
 * @return The parsed JSON object.
 */
nlohmann::json parse(std::string_view jsonString, bool allowComments = false);

/**
 * Dump a JSON object to a string.
 *
 * As with parse, this avoids code-generating an entire JSON serializer repeatedly.
 *
 * @param json The JSON object to serialize.
 * @param indent The indentation level to use. The meaning is the same as with nlohmann::json::dump: -1 is maximally
 *               compact, otherwise newlines are emitted, and the given number of spaces is used for each level of
 *               indentation.
 * @return A string with the JSON encoded object.
 */
std::string dump(const nlohmann::json &json, int indent = -1);

} // namespace Json

/// @}
