#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace mdp::publisher::json
{
    class Value
    {
    public:
        enum class Type
        {
            Null,
            Boolean,
            Number,
            String,
            Array,
            Object
        };

        using Array = std::vector<Value>;
        using Object = std::unordered_map<std::string, Value>;

    public:
        Value() = default;

        static Value makeBoolean(bool value);
        static Value makeNumber(double value);
        static Value makeString(std::string value);
        static Value makeArray(Array value);
        static Value makeObject(Object value);

        Type type() const { return m_type; }
        bool isNull() const { return m_type == Type::Null; }
        bool isBoolean() const { return m_type == Type::Boolean; }
        bool isNumber() const { return m_type == Type::Number; }
        bool isString() const { return m_type == Type::String; }
        bool isArray() const { return m_type == Type::Array; }
        bool isObject() const { return m_type == Type::Object; }

        bool asBoolean() const;
        double asNumber() const;
        const std::string& asString() const;
        const Array& asArray() const;
        const Object& asObject() const;

    private:
        Type m_type{ Type::Null };
        bool m_booleanValue{ false };
        double m_numberValue{ 0.0 };
        std::string m_stringValue;
        Array m_arrayValue;
        Object m_objectValue;
    };

    class Parser
    {
    public:
        explicit Parser(std::string_view input);

        Value parse();

    private:
        Value parseValue();
        Value parseObject();
        Value parseArray();
        Value parseStringValue();
        Value parseNumberValue();
        bool parseLiteral(std::string_view literal);
        std::string parseString();
        double parseNumber();
        void skipWhitespace();
        char peek() const;
        char consume();
        void expect(char expected);
        [[noreturn]] void fail(const std::string& message) const;

    private:
        std::string_view m_input;
        std::size_t m_index{ 0 };
    };

    Value parse(std::string_view input);
}
