#include "Json.hpp"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace cm::json
{
const Value* Value::Find(const std::string& key) const
{
    if (type != Type::Object)
        return nullptr;
    auto it = object.find(key);
    return it == object.end() ? nullptr : &it->second;
}

namespace
{
struct Parser
{
    const char* p;
    const char* end;

    void SkipWs()
    {
        while (p < end)
        {
            const char c = *p;
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
                ++p;
            else if (c == '/' && p + 1 < end && p[1] == '/') // tolerate // comments
            {
                while (p < end && *p != '\n')
                    ++p;
            }
            else
                break;
        }
    }

    bool ParseValue(Value& out)
    {
        SkipWs();
        if (p >= end)
            return false;
        switch (*p)
        {
        case '{': return ParseObject(out);
        case '[': return ParseArray(out);
        case '"': return ParseString(out);
        case 't': case 'f': return ParseBool(out);
        case 'n': return ParseNull(out);
        default: return ParseNumber(out);
        }
    }

    bool ParseString(Value& out)
    {
        out.type = Value::Type::String;
        return ReadRawString(out.string);
    }

    bool ReadRawString(std::string& s)
    {
        if (p >= end || *p != '"')
            return false;
        ++p;
        s.clear();
        while (p < end && *p != '"')
        {
            char c = *p++;
            if (c == '\\' && p < end)
            {
                char e = *p++;
                switch (e)
                {
                case 'n': s += '\n'; break;
                case 't': s += '\t'; break;
                case 'r': s += '\r'; break;
                case 'b': s += '\b'; break;
                case 'f': s += '\f'; break;
                case '/': s += '/'; break;
                case '\\': s += '\\'; break;
                case '"': s += '"'; break;
                case 'u': // pass through basic ASCII; ignore full unicode decoding
                    if (p + 4 <= end)
                    {
                        const std::string hex(p, p + 4);
                        const long code = strtol(hex.c_str(), nullptr, 16);
                        if (code < 0x80)
                            s += static_cast<char>(code);
                        p += 4;
                    }
                    break;
                default: s += e; break;
                }
            }
            else
            {
                s += c;
            }
        }
        if (p >= end)
            return false;
        ++p; // closing quote
        return true;
    }

    bool ParseNumber(Value& out)
    {
        const char* start = p;
        if (p < end && (*p == '-' || *p == '+'))
            ++p;
        bool any = false;
        while (p < end && (std::isdigit(static_cast<unsigned char>(*p)) || *p == '.' || *p == 'e' || *p == 'E' ||
                           *p == '+' || *p == '-'))
        {
            ++p;
            any = true;
        }
        if (!any)
            return false;
        out.type = Value::Type::Number;
        out.number = strtod(std::string(start, p).c_str(), nullptr);
        return true;
    }

    bool ParseBool(Value& out)
    {
        if (end - p >= 4 && std::string(p, p + 4) == "true")
        {
            out.type = Value::Type::Bool;
            out.boolean = true;
            p += 4;
            return true;
        }
        if (end - p >= 5 && std::string(p, p + 5) == "false")
        {
            out.type = Value::Type::Bool;
            out.boolean = false;
            p += 5;
            return true;
        }
        return false;
    }

    bool ParseNull(Value& out)
    {
        if (end - p >= 4 && std::string(p, p + 4) == "null")
        {
            out.type = Value::Type::Null;
            p += 4;
            return true;
        }
        return false;
    }

    bool ParseArray(Value& out)
    {
        out.type = Value::Type::Array;
        ++p; // [
        SkipWs();
        if (p < end && *p == ']')
        {
            ++p;
            return true;
        }
        while (p < end)
        {
            Value v;
            if (!ParseValue(v))
                return false;
            out.array.push_back(std::move(v));
            SkipWs();
            if (p < end && *p == ',')
            {
                ++p;
                continue;
            }
            if (p < end && *p == ']')
            {
                ++p;
                return true;
            }
            return false;
        }
        return false;
    }

    bool ParseObject(Value& out)
    {
        out.type = Value::Type::Object;
        ++p; // {
        SkipWs();
        if (p < end && *p == '}')
        {
            ++p;
            return true;
        }
        while (p < end)
        {
            SkipWs();
            std::string key;
            if (!ReadRawString(key))
                return false;
            SkipWs();
            if (p >= end || *p != ':')
                return false;
            ++p;
            Value v;
            if (!ParseValue(v))
                return false;
            out.object.emplace(std::move(key), std::move(v));
            SkipWs();
            if (p < end && *p == ',')
            {
                ++p;
                continue;
            }
            if (p < end && *p == '}')
            {
                ++p;
                return true;
            }
            return false;
        }
        return false;
    }
};
} // namespace

bool Parse(const std::string& text, Value& out)
{
    Parser parser{text.c_str(), text.c_str() + text.size()};
    if (!parser.ParseValue(out))
        return false;
    parser.SkipWs();
    return true; // trailing content tolerated
}

bool ParseFile(const std::string& path, Value& out)
{
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open())
        return false;
    std::stringstream ss;
    ss << f.rdbuf();
    return Parse(ss.str(), out);
}
} // namespace cm::json
