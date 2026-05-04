#include "ibkr/IbkrQuoteParser.h"

#include "ibkr/SimpleJson.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <sstream>

namespace mdp::publisher
{
    namespace
    {
        std::optional<double> tryGetNumber(
            const json::Value::Object& object,
            const std::string& key)
        {
            const auto it = object.find(key);
            if (it == object.end())
            {
                return std::nullopt;
            }

            if (it->second.isNumber())
            {
                return it->second.asNumber();
            }

            if (!it->second.isString())
            {
                return std::nullopt;
            }

            std::string text = it->second.asString();
            text.erase(
                std::remove_if(
                    text.begin(),
                    text.end(),
                    [](unsigned char c)
                    {
                        return std::isspace(c) != 0 || c == ',';
                    }),
                text.end());

            if (text.empty() || text == "-" || text == "N/A")
            {
                return std::nullopt;
            }

            try
            {
                return std::stod(text);
            }
            catch (const std::exception&)
            {
                return std::nullopt;
            }
        }

        std::optional<std::uint64_t> tryGetUint64(
            const json::Value::Object& object,
            const std::string& key)
        {
            const auto number = tryGetNumber(object, key);
            if (!number.has_value() || *number < 0.0)
            {
                return std::nullopt;
            }

            return static_cast<std::uint64_t>(*number);
        }

        std::optional<std::string> tryGetString(
            const json::Value::Object& object,
            const std::string& key)
        {
            const auto it = object.find(key);
            if (it == object.end())
            {
                return std::nullopt;
            }

            if (it->second.isString())
            {
                return it->second.asString();
            }

            if (it->second.isNumber())
            {
                std::ostringstream output;
                output << static_cast<std::uint64_t>(it->second.asNumber());
                return output.str();
            }

            return std::nullopt;
        }

        std::optional<IbkrQuote> parseQuoteObject(const json::Value::Object& object)
        {
            const std::optional<std::string> conid =
                tryGetString(object, "conid").has_value()
                ? tryGetString(object, "conid")
                : tryGetString(object, "conidEx");
            if (!conid.has_value() || conid->empty())
            {
                return std::nullopt;
            }

            IbkrQuote quote;
            quote.conid = *conid;
            quote.lastPrice = tryGetNumber(object, "31");
            quote.bidPrice = tryGetNumber(object, "84");
            quote.askPrice = tryGetNumber(object, "86");
            const auto bidSize = tryGetUint64(object, "85");
            if (bidSize.has_value())
            {
                quote.bidSize = static_cast<std::uint32_t>(*bidSize);
            }
            const auto askSize = tryGetUint64(object, "88");
            if (askSize.has_value())
            {
                quote.askSize = static_cast<std::uint32_t>(*askSize);
            }
            quote.updatedMs = tryGetUint64(object, "_updated");
            return quote;
        }
    }

    std::vector<IbkrQuote> parseIbkrSnapshotPayload(const std::string& payload)
    {
        if (payload.empty())
        {
            return {};
        }

        const json::Value root = json::parse(payload);
        if (!root.isArray())
        {
            return {};
        }

        std::vector<IbkrQuote> quotes;
        for (const auto& item : root.asArray())
        {
            if (!item.isObject())
            {
                continue;
            }

            const auto quote = parseQuoteObject(item.asObject());
            if (quote.has_value())
            {
                quotes.push_back(*quote);
            }
        }

        return quotes;
    }

    std::optional<IbkrQuote> parseIbkrStreamingPayload(const std::string& payload)
    {
        if (payload.empty())
        {
            return std::nullopt;
        }

        const json::Value root = json::parse(payload);
        if (!root.isObject())
        {
            return std::nullopt;
        }

        return parseQuoteObject(root.asObject());
    }

    void mergeIbkrQuote(IbkrQuote& target, const IbkrQuote& update)
    {
        if (!update.conid.empty())
        {
            target.conid = update.conid;
        }

        if (update.lastPrice.has_value())
        {
            target.lastPrice = update.lastPrice;
        }

        if (update.bidPrice.has_value())
        {
            target.bidPrice = update.bidPrice;
        }

        if (update.askPrice.has_value())
        {
            target.askPrice = update.askPrice;
        }

        if (update.bidSize.has_value())
        {
            target.bidSize = update.bidSize;
        }

        if (update.askSize.has_value())
        {
            target.askSize = update.askSize;
        }

        if (update.updatedMs.has_value())
        {
            target.updatedMs = update.updatedMs;
        }
    }
}
