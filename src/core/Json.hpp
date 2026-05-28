#pragma once
#include <map>
#include <string>
#include <vector>

// Tiny dependency-free JSON reader, sufficient for the theme files
// (objects, arrays, strings, numbers, bools, null). Read-only.
namespace cm::json
{
class Value
{
public:
    enum class Type
    {
        Null,
        Bool,
        Number,
        String,
        Array,
        Object,
    };

    Type type = Type::Null;
    bool boolean = false;
    double number = 0.0;
    std::string string;
    std::vector<Value> array;
    std::map<std::string, Value> object;

    bool IsObject() const { return type == Type::Object; }
    bool IsArray() const { return type == Type::Array; }
    bool IsNumber() const { return type == Type::Number; }
    bool IsString() const { return type == Type::String; }

    // Object lookup (nullptr if missing / not an object).
    const Value* Find(const std::string& key) const;

    double AsNumber(double def = 0.0) const { return type == Type::Number ? number : def; }
    bool AsBool(bool def = false) const { return type == Type::Bool ? boolean : def; }
    std::string AsString(const std::string& def = "") const { return type == Type::String ? string : def; }
};

// Parse JSON text. Returns false on syntax error.
bool Parse(const std::string& text, Value& out);

// Read + parse a file. Returns false if it can't be read or parsed.
bool ParseFile(const std::string& path, Value& out);
} // namespace cm::json
