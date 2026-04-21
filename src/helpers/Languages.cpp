/*
 * Copyright 2023-2026, Andrea Anzani
 * Copyright 2014-2018 Kacper Kasper  (from Koder editor)
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "Languages.h"

#include <algorithm>
#include <functional>
#include <map>
#include <string>

#include <Catalog.h>
#include <Directory.h>
#include <FindDirectory.h>
#include <Path.h>
#include <String.h>

#include <ILexer.h>
#include <Lexilla.h>
#include <SciLexer.h>
#include <yaml-cpp/yaml.h>

#include "Editor.h"
#include "Log.h"
#include "Utils.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Languages"


std::vector<std::string>			Languages::sLanguages;
std::map<std::string, std::string>	Languages::sMenuItems;
std::map<std::string, std::string> 	Languages::sExtensions;


namespace {

void
DoInAllDataDirectories(std::function<void(const BPath&)> func) {
	func(GetNearbyDataDirectory());
	func(GetDataDirectoryByWhich(B_USER_SETTINGS_DIRECTORY));
	func(GetDataDirectoryByWhich(B_USER_NONPACKAGED_DATA_DIRECTORY));
	func(GetDataDirectoryByWhich(B_SYSTEM_NONPACKAGED_DATA_DIRECTORY));
	func(GetDataDirectoryByWhich(B_SYSTEM_DATA_DIRECTORY));
}

void
DoInAllLibDirectories(std::function<void(const BPath&)> func) {
	BPath libPath;
	find_directory(B_SYSTEM_LIB_DIRECTORY, &libPath);
	func(libPath);
	find_directory(B_USER_LIB_DIRECTORY, &libPath);
	func(libPath);
	find_directory(B_SYSTEM_NONPACKAGED_LIB_DIRECTORY, &libPath);
	func(libPath);
	find_directory(B_USER_NONPACKAGED_LIB_DIRECTORY, &libPath);
	func(libPath);
}

class LexerLibrary {
public:
	LexerLibrary(const char* path) {
		fLibrary = load_add_on(path);
		if(fLibrary > 0) {
			if(get_image_symbol(fLibrary, LEXILLA_CREATELEXER, B_SYMBOL_TYPE_TEXT,
					reinterpret_cast<void**>(&fCreateLexer)) != B_OK) {
				fCreateLexer = nullptr;
			}
		}
	}

	~LexerLibrary() {
		if(fLibrary > 0) {
			unload_add_on(fLibrary);
		}
		fLibrary = 0;
	}

	bool IsValid() {
		return fLibrary > 0 && fCreateLexer != nullptr;
	}

	Scintilla::ILexer5* CreateLexer(const char* name) {
		return fCreateLexer(name);
	}

private:
	image_id fLibrary;
	Lexilla::CreateLexerFn fCreateLexer;
};

std::vector<std::unique_ptr<LexerLibrary>> sLexerLibraries;

}

/* static */ bool
Languages::GetLanguageForExtension(const std::string& ext, std::string& lang)
{
	lang = "text";
	if (sExtensions.count(ext) > 0) {
		lang = sExtensions[ext];
		return true;
	}
	return false;
}


/* static */ void
Languages::SortAlphabetically()
{
	std::sort(sLanguages.begin(), sLanguages.end());
}


/**
 * Reads YAML files from all data directories and creates a single style map,
 * where repeated keys are overridden.
 */
/* static */ std::map<int, int>
Languages::ApplyLanguage(Editor* editor, const char* lang)
{
	editor->SendMessage(SCI_FREESUBSTYLES);
	std::map<int, int> styleMapping;
	DoInAllDataDirectories([&](const BPath& path) {
			try {
				auto m = _ApplyLanguage(editor, lang, path);
				m.merge(styleMapping);
				std::swap(styleMapping, m);
			} catch (const YAML::BadFile &) {}
		});
	return styleMapping;
}

/**
 * Loads YAML file with language specification:
 *   lexer: string (required)
 *   properties: (string|string) map -> SCI_SETPROPERTY
 *   keywords: (index(int)|string) map -> SCI_SETKEYWORDS
 *   identifiers: (lexem class id(int)|(string array)) map -> SCI_SETIDENTIFIERS
 *   comments:
 *     line: string
 *     block: pair of strings
 *   styles: (lexem class id(int)|Koder style id(int)) map
 *   substyles: (lexem class id(int)|(Koder style id(int)) array) map
 *
 * For substyles, strings in identifiers array are matched with styles in
 * substyles array. Array instead of map is used because substyles are allocated
 * contiguously. Theoretically there is no limit on how many substyles there
 * can be. Substyling of lexem class id must be supported by the lexer.
 *
 * Substyle ids are created using starting id returned by SCI_ALLOCATESUBSTYLE.
 * For example if it returns 128, then 1st id = 128, 2nd = 129, 3rd = 130.
 * These are then passed to SCI_SETIDENTIFIERS and merged into regular styles
 * map to be handled by the Styler class.
 */
/* static */ std::map<int, int>
Languages::_ApplyLanguage(Editor* editor, const char* lang, const BPath &path)
{

	LogDebug("Applying language %s using path %s\n", lang, path.Path());

	if(sLexerLibraries.empty() == true)
		return {};
	// TODO: early exit if lexer not changed

	BPath p = path;
	p.Append("languages");
	p.Append(lang);
	BString fileName(p.Path());
	fileName.Append(".yaml");

	if (!BEntry(fileName.String()).Exists()) {
		// TODO: Workaround for a bug in Haiku x86_32: exceptions
		// thrown inside yaml_cpp aren't catchable. We throw this exception
		// inside Genio and that works.
		throw YAML::BadFile(fileName.String());
	}
	const YAML::Node language = YAML::LoadFile(std::string(p.Path()) + ".yaml");
	std::string lexerName = language["lexer"].as<std::string>();
	Scintilla::ILexer5* lexer = nullptr;
	// sLexerLibraries contains libraries in the following order:
	// * system
	// * user
	// * non-packaged system
	// * non-packaged user
	// Going in reverse results in correct override hierarchy.
	for (auto it = sLexerLibraries.rbegin(); it != sLexerLibraries.rend(); ++it) {
		lexer = (*it)->CreateLexer(lexerName.c_str());
		if (lexer != nullptr)
			break;
	}

	if (lexer == nullptr)
		return std::map<int, int>();

	editor->SendMessage(SCI_SETILEXER, 0, reinterpret_cast<sptr_t>(lexer));

	for (const auto& property : language["properties"]) {
		auto name = property.first.as<std::string>();
		auto value = property.second.as<std::string>();
		editor->SendMessage(SCI_SETPROPERTY, (uptr_t) name.c_str(), (sptr_t) value.c_str());
	}

	for (const auto& keyword : language["keywords"]) {
		auto num = keyword.first.as<int>();
		auto words = keyword.second.as<std::string>();
		editor->SendMessage(SCI_SETKEYWORDS, num, (sptr_t) words.c_str());
	}

	std::unordered_map<int, int> substyleStartMap;
	const auto& identifiers = language["identifiers"];
	if (identifiers && identifiers.IsMap()) {
		for (const auto& id : identifiers) {
			if (!id.second.IsSequence())
				continue;

			const int substyleId = id.first.as<int>();
			// TODO: allocate only once
			const uptr_t start = editor->SendMessage(SCI_ALLOCATESUBSTYLES,
				uptr_t(substyleId), sptr_t(id.second.size()));
			substyleStartMap.emplace(substyleId, start);
			int i = 0;
			for (const auto& idents : id.second) {
				editor->SendMessage(SCI_SETIDENTIFIERS, start + i++,
					reinterpret_cast<sptr_t>(idents.as<std::string>().c_str()));
			}
		}
	}

	const YAML::Node comments = language["comments"];

	if (comments) {
		const YAML::Node line = comments["line"];
		if (line)
			editor->SetCommentLineToken(line.as<std::string>());
		const YAML::Node block = comments["block"];
		if (block && block.IsSequence())
			editor->SetCommentBlockTokens(block[0].as<std::string>(),
				block[1].as<std::string>());
	}

	std::map<int, int> styleMap;
	const YAML::Node styles = language["styles"];
	if (styles) {
		styleMap = styles.as<std::map<int, int>>();
	}
	const YAML::Node substyles = language["substyles"];
	if (substyles && substyles.IsMap()) {
		for (const auto& id : substyles) {
			if (!id.second.IsSequence())
				continue;

			int i = 0;
			for (const auto& styleId : id.second) {
				const int substyleStart = substyleStartMap[id.first.as<int>()];
				styleMap.emplace(substyleStart + i++, styleId.as<int>());
			}
		}
	}
	return styleMap;
}


/* static */ void
Languages::LoadLanguages()
{
	DoInAllDataDirectories([](const BPath& path) {
			try {
				_LoadLanguages(path);
			} catch (const YAML::BadFile &) {}
		});

	DoInAllLibDirectories([](const BPath& path) {
		BPath p(path);
		p.Append(LEXILLA_LIB LEXILLA_EXTENSION);
		auto lexilla = std::make_unique<LexerLibrary>(p.Path());
		if(lexilla->IsValid() == true) {
			sLexerLibraries.push_back(std::move(lexilla));
		}
	});

	DoInAllLibDirectories([](const BPath& path) {
		BPath p(path);
		p.Append("lexilla");
		BDirectory lexersDir(p.Path());
		if (lexersDir.InitCheck() != B_OK)
			return;

		BEntry lexerEntry;
		while(lexersDir.GetNextEntry(&lexerEntry, true) == B_OK) {
			if(lexerEntry.IsDirectory())
				continue;
			BPath lexerPath;
			lexerEntry.GetPath(&lexerPath);
			auto lexer = std::make_unique<LexerLibrary>(lexerPath.Path());
			if(lexer->IsValid() == true) {
				sLexerLibraries.push_back(std::move(lexer));
			}
		}
	});

	Languages::SortAlphabetically();
}


/* static */ void
Languages::_LoadLanguages(const BPath& path)
{
	BPath p(path);
	p.Append("languages");

	BDirectory languages(p.Path());
	if (languages.InitCheck() != B_OK) {
		LogError("Can't reading the language directory: %s", p.Path());
		return;
	}
	LogDebug("Reading the language directory: %s\n", p.Path());
	entry_ref ref;
	while(languages.GetNextRef(&ref) == B_OK) {

		LogTrace("--> Language file: %s\n", ref.name);

		std::string name(ref.name);
		if (name.ends_with(".yaml") == false) {
			LogTrace("    invalid filename: %s\n", ref.name);
			continue;
		}

		name.resize(name.size() - 5);

		//The configuration is already present.
		if(std::find(sLanguages.begin(), sLanguages.end(), name) != sLanguages.end()) {
			LogTrace("    configuration already loaded: %s\n", ref.name);
			continue;
		}

		BEntry entry(&ref);
		if (entry.InitCheck() == B_OK && entry.IsFile()) {
			BPath languageFile(&ref);
			LogTrace("--> Language file: %s\n", languageFile.Path());

			try {
				const YAML::Node lang = YAML::LoadFile(languageFile.Path());
				std::string menuname = lang["name"].as<std::string>();
				auto extensions = lang["extensions"].as<std::vector<std::string>>();
				for (const auto& ext : extensions) {
					sExtensions[ext] = name;
					LogTrace("Extension [%s] for language [%s]\n", ext.c_str(), name.c_str());
				}

				sLanguages.push_back(name);
				sMenuItems[name] = menuname;

			} catch (const YAML::Exception & e)  {
				LogError("Error reading %s (%s)\n", languageFile.Path(), e.msg.c_str());
			}
		}
	}
}