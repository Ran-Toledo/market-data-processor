#include "ibkr/SimpleJson.h"

#include <cctype>
#include <cmath>
#include <stdexcept>

namespace mdp::publisher::json
{
    Value Value::makeBoolean(bool value)
    {
        Value jsonValue;
        jsonValue.m_type = Type::Boolean;
        jsonValue.m_booleanValue = value;
        return jsonValue;
    }

    Value Value::makeNumber(double value)
    {
        Value jsonValue;
        jsonValue.m_type = Type::Number;
        jsonValue.m_numberValue = value;
        return jsonValue;
    }

    Value Value::makeString(std::string value)
    {
        Value jsonValue;
        jsonValue.m_type = Type::String;
        jsonValue.m_stringValue = std::move(value);
        return jsonValue;
    }

    Value Value::makeArray(Array value)
    {
        Value jsonValue;
        jsonValue.m_type = Type::Array;
        jsonValue.m_arrayValue = std::move(value);
        return jsonValue;
    }

    Value Value::makeObject(Object value)
    {
        Value jsonValue;
        jsonValue.m_type = Type::Object;
        jsonValue.m_objectValue = std::move(value);
        return jsonValue;
    }

    bool Value::asBoolean() const
    {
        if (!isBoolean())
        {
            throw std::runtime_error("JSON value is not a boolean");
        }

        return m_booleanValue;
    }

    double Value::asNumber() const
    {
        if (!isNumber())
        {
            throw std::runtime_error("JSON value is not a number");
        }

        return m_numberValue;
    }

    const std::string& Value::asString() const
    {
        if (!isString())
        {
            throw std::runtime_error("JSON value is not a string");
        }

        return m_stringValue;
    }

    const Value::Array& Value::asArray() const
    {
        if (!isArray())
        {
            throw std::runtime_error("JSON value is not an array");
        }

        return m_arrayValue;
    }

    const Value::Object& Value::asObject() const
    {
        if (!isObject())
        {
            throw std::runtime_error("JSON value is not an object");
        }

        return m_objectValue;
    }

    Parser::Parser(std::string_view input)
        : m_input(input)
    {
    }

    Value Parser::parse()
    {
        skipWhitespace();
        Value root = parseValue();
        skipWhitespace();

        if (m_index != m_input.size())
        {
            fail("Unexpected trailing content");
        }

        return root;
    }

    Value Parser::parseValue()
    {
        skipWhitespace();
        const char current = peek();

        switch (current)
        {
        case '{':
            return parseObject();
        case '[':
            return parseArray();
        case '"':
            return parseStringValue();
        case 't':
            if (parseLiteral("true"))
            {
                return Value::makeBoolean(true);
            }
            break;
        case 'f':
            if (parseLiteral("false"))
            {
                return Value::makeBoolean(false);
            }
            break;
        case 'n':
            if (parseLiteral("null"))
            {
                return Value();
            }
            break;
        default:
            if (current == '-' || std::isdigit(static_cast<unsigned char>(current)) != 0)
            {
                return parseNumberValue();
            }
            break;
        }

        fail("Invalid JSON value");
    }

    Value Parser::parseObject()
    {
        expect('{');
        skipWhitespace();

        Value::Object object;
        if (peek() == '}')
        {
            consume();
            return Value::makeObject(std::move(object));
        }

        while (true)
        {
            skipWhitespace();
            const std::string key = parseString();
            skipWhitespace();
            expect(':');
            skipWhitespace();
            object.emplace(key, parseValue());
            skipWhitespace();

            const char separator = consume();
            if (separator == '}')
            {
                break;
            }

            if (separator != ',')
            {
                fail("Expected ',' or '}' in object");
            }
        }

        return Value::makeObject(std::move(object));
    }

    Value Parser::parseArray()
    {
        expect('[');
        skipWhitespace();

        Value::Array array;
        if (peek() == ']')
        {
            consume();
            return Value::makeArray(std::move(array));
        }

        while (true)
        {
            array.push_back(parseValue());
            skipWhitespace();

            const char separator = consume();
            if (separator == ']')
            {
                break;
            }

            if (separator != ',')
            {
                fail("Expected ',' or ']' in array");
            }
        }

        return Value::makeArray(std::move(array));
    }

    Value Parser::parseStringValue()
    {
        return Value::makeString(parseString());
    }

    Value Parser::parseNumberValue()
    {
        return Value::makeNumber(parseNumber());
    }

    bool Parser::parseLiteral(std::string_view literal)
    {
        if (m_input.substr(m_index, literal.size()) != literal)
        {
            return false;
        }

        m_index += literal.size();
        return true;
    }

    std::string Parser::parseString()
    {
        expect('"');

        std::string result;
        while (m_index < m_input.size())
        {
            const char c = consume();
            if (c == '"')
            {
                return result;
            }

            if (c == '\\')
            {
                const char escaped = consume();
                switch (escaped)
                {
                case '"':
                case '\\':
                case '/':
                    result.push_back(escaped);
                    break;
                case 'b':
                    result.push_back('\b');
                    break;
                case 'f':
                    result.push_back('\f');
                    break;
                case 'n':
                    result.push_back('\n');
                    break;
                case 'r':
                    result.push_back('\r');
                    break;
                case 't':
                    result.push_back('\t');
                    break;
                case 'u':
                {
                    if (m_index + 4 > m_input.size())
                    {
                        fail("Invalid unicode escape");
                    }

                    unsigned int codePoint = 0;
                    for (int i = 0; i < 4; ++i)
                    {
                        const char hex = consume();
                        codePoint <<= 4;
                        if (hex >= '0' && hex <= '9')
                        {
                            codePoint |= static_cast<unsigned int>(hex - '0');
                        }
                        else if (hex >= 'a' && hex <= 'f')
                        {
                            codePoint |= static_cast<unsigned int>(hex - 'a' + 10);
                        }
                        else if (hex >= 'A' && hex <= 'F')
                        {
                            codePoint |= static_cast<unsigned int>(hex - 'A' + 10);
                        }
                        else
                        {
                            fail("Invalid unicode escape");
                        }
                    }

                    result.push_back(codePoint <= 0x7F ?
                        static_cast<char>(codePoint) :
                        '?');
                    break;
                }
                default:
                    fail("Invalid string escape");
                }

                continue;
            }

            result.push_back(c);
        }

        fail("Unterminated string");
    }

    double Parser::parseNumber()
    {
        const std::size_t start = m_index;

        if (peek() == '-')
        {
            consume();
        }

        while (std::isdigit(static_cast<unsigned char>(peek())) != 0)
        {
            consume();
        }

        if (peek() == '.')
        {
            consume();
            while (std::isdigit(static_cast<unsigned char>(peek())) != 0)
            {
                consume();
            }
        }

        if (peek() == 'e' || peek() == 'E')
        {
            consume();
            if (peek() == '+' || peek() == '-')
            {
                consume();
            }

            while (std::isdigit(static_cast<unsigned char>(peek())) != 0)
            {
                consume();
            }
        }

        const std::string text(m_input.substr(start, m_index - start));
        try
        {
            return std::stod(text);
        }
        catch (const std::exception&)
        {
            fail("Invalid number");
        }
    }

    void Parser::skipWhitespace()
    {
        while (m_index < m_input.size() &&
            std::isspace(static_cast<unsigned char>(m_input[m_index])) != 0)
        {
            ++m_index;
        }
    }

    char Parser::peek() const
    {
        if (m_index >= m_input.size())
        {
            return '\0';
        }

        return m_input[m_index];
    }

    char Parser::consume()
    {
        if (m_index >= m_input.size())
        {
            fail("Unexpected end of input");
        }

        return m_input[m_index++];
    }

    void Parser::expect(char expected)
    {
        const char actual = consume();
        if (actual != expected)
        {
            fail(std::string("Expected '") + expected + "'");
        }
    }

    [[noreturn]] void Parser::fail(const std::string& message) const
    {
        throw std::runtime_error(
            "JSON parse error at offset " + std::to_string(m_index) +
            ": " + message);
    }

    Value parse(std::string_view input)
    {
        Parser parser(input);
        return parser.parse();
    }
}
