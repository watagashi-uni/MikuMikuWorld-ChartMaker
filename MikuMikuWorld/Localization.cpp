#include "Localization.h"
#include "DefaultLanguage.h"
#include "IO.h"
#include "File.h"
#include <algorithm>
#include <cctype>
#include <filesystem>

namespace MikuMikuWorld
{
	static std::string empty;

	namespace
	{
		std::string normalizeLanguageCode(std::string code)
		{
			std::replace(code.begin(), code.end(), '_', '-');

			const size_t end = code.find_first_of(".@");
			if (end != std::string::npos)
				code = code.substr(0, end);

			std::transform(code.begin(), code.end(), code.begin(), [](unsigned char ch)
			{
				return (char)std::tolower(ch);
			});

			return code;
		}

		std::string canonicalLanguageCode(std::string code)
		{
			code = normalizeLanguageCode(std::move(code));

			if (code == "zh" || code == "zh-cn" || code == "zh-sg" || code == "zh-hans")
				return "zh-cn";

			if (code == "zh-tw" || code == "zh-hk" || code == "zh-mo" || code == "zh-hant")
				return "zh-tw";

			return code;
		}
	}

	std::map<std::string, std::unique_ptr<Language>> Localization::languages;
	Language* Localization::currentLanguage = nullptr;

	void Localization::load(const char* code, std::string name, const std::string& filename)
	{
		if (!IO::File::exists(filename))
			return;

        languages[code] = std::make_unique<Language>(code, name, filename);
	}

	bool Localization::setLanguage(const std::string& code)
	{
		auto it = Localization::languages.find(code);
		if (it != Localization::languages.end())
		{
			Localization::currentLanguage = it->second.get();
			return true;
		}

		std::string normalized = canonicalLanguageCode(code);
		it = Localization::languages.find(normalized);
		if (it != Localization::languages.end())
		{
			Localization::currentLanguage = it->second.get();
			return true;
		}

		size_t separator = normalized.find('-');
		if (separator != std::string::npos)
		{
			it = Localization::languages.find(normalized.substr(0, separator));
			if (it != Localization::languages.end())
			{
				Localization::currentLanguage = it->second.get();
				return true;
			}
		}

		return false;
	}

	void Localization::loadDefault()
	{
		languages["en"] = std::make_unique<Language>("en", "English", en);
	}

	const char* getString(const std::string& key)
	{
		if (!Localization::currentLanguage)
			return key.c_str();

        return Localization::currentLanguage->getString(key);
	}
}
