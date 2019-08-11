/**
 * \file WindowsSpellChecker.cpp
 * This file is part of LyX, the document processor.
 * License details can be found in the file COPYING.
 *
 * \author Niko Strijbol
 *
 * Full author contact details are available in file CREDITS.
 */
#include "WindowsSpellChecker.h"
#include "PersonalWordList.h"

#include "WordLangTuple.h"

#include "support/debug.h"
#include "support/docstring_list.h"
#include "support/Package.h"
#include "support/os.h"
#include "support/unicode.h"

#include <vector>
#include <algorithm>
#include <codecvt>
#include <string>

#include <spellcheck.h>
#include <cstdio>
#include <iostream>
#include <tchar.h>


using namespace lyx::support;

namespace lyx {

/**
 * Convert the language to an approximate BCP47 language tag.
 *
 * The resulting tag will be in UTF-16, which is what Windows wants.
 *
 * @todo This should be less approximate and more exact somehow.
 *
 * Note: this function assumes the std::strings in Language are UTF-8 (or ANSI), which
 * they probably are.
 *
 * @param lang The language to convert.
 * @return The UTF-16 encoded BCP47 language tag.
 */
std::wstring langToBcp47(Language const *lang);

/**
 * Convert a UTF-32 (UCS4) string to UTF-16.
 *
 * Basically convert a docstring for input in a Win32 api. This function can be used to
 * convert a docstring to a PCWSTR. Simply get the C-style string from the result:
 * @code
 * std::wstring original = L"test";
 * PCWSTR windowsString = original.c_str();
 * @endcode
 *
 * Docstring is the internal LyX type for UTF-32 (UCS4). At one point in the future,
 * it should be possible to use std::u32string, which is basically the same thing.
 *
 * @param source Original, UTF-32 (UCS4) docstring.
 * @return The UTF-16 encoded string.
 */
std::wstring to_utf16(const docstring &source);

/**
 * Convert a UTF-16 string to UTF-32.
 *
 * Basically convert Win32 output to a docstring. This function can be used to convert a
 * LPOLESTR to a docstring. To do this, first convert the LPOLESTR to a std::wstring:
 * @code
 * LPOLESTR original = ...
 * std::wstring wideOriginal(original);
 * @endcode
 * This is works because LPOLESTR is defined as an array of OLECHAR, which is a typedef
 * for WCHAR, which is a typedef for wchar_t, which is 16-bit on Windows. Similarly,
 * std::wstring used 16-bit chars.
 *
 * Docstring is the internal LyX type for UTF-32 (UCS4). At one point in the future,
 * it should be possible to use std::u32string, which is basically the same thing.
 *
 * @param source Original, UTF-16 encoded string.
 * @return The UTF-32 encoded docstring.
 */
docstring from_utf16(const std::wstring &source);

/**
 * Map language (lang->lang()) to the speller.
 */
typedef std::map<std::string, ISpellChecker *> Spellers;

struct WindowsSpellChecker::Private {
	Private();

	~Private();

	/// Communication
	/// bool wasCOMInitialized;s are initialized or not.
	/// Factory to get Spellers from.
	ISpellCheckerFactory *factory;
	/// List of initialized spellers.
	Spellers spellers;

	/// Get a speller (or nullptr) for a language.
	ISpellChecker *getOrCreate(Language const *lang);
};

WindowsSpellChecker::Private::Private()
{
	LYXERR0("Starting spell checker.");
	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	factory = nullptr;
	// Qt5 already initialises COM for us. Other frontend might not do this, so keep
	// the code here, but ignore errors about double initialisation.
	if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
		LYXERR0("COM not initialized successfully.");
		return;
	}

	hr = CoCreateInstance(__uuidof(SpellCheckerFactory), nullptr,
	                      CLSCTX_INPROC_SERVER,
	                      IID_PPV_ARGS(&factory));
	if (FAILED(hr)) {
		LYXERR0("Init error");
		factory = nullptr;
		return;
	}
}

WindowsSpellChecker::Private::~Private()
{
	LYXERR0("Destroying");
	for (std::pair<std::string, ISpellChecker *> speller : spellers) {
		if (speller.second != nullptr) {
			speller.second->Release();
		}
	}
	spellers.clear();
	if (factory != nullptr) {
		factory->Release();
	}
	LYXERR0("Un-initializing com");
	CoUninitialize();
}

ISpellChecker *WindowsSpellChecker::Private::getOrCreate(Language const *lang)
{
	// If the speller is not available, add it.
	if (spellers.find(lang->lang()) == spellers.end()) {
		BOOL isSupported = FALSE;
		std::wstring tag = langToBcp47(lang);
		HRESULT hr = factory->IsSupported(tag.c_str(), &isSupported);
		if (SUCCEEDED(hr)) {
			if (isSupported == FALSE) {
				spellers.emplace(lang->lang(), nullptr);
			} else {
				ISpellChecker *spellChecker = nullptr;
				factory->CreateSpellChecker(tag.c_str(), &spellChecker);
				spellers.emplace(lang->lang(), spellChecker);
			}
		} else {

			spellers.emplace(lang->lang(), nullptr);
		}
	}
	return spellers.at(lang->lang());
}

WindowsSpellChecker::WindowsSpellChecker()
{
	d = new Private();
}

WindowsSpellChecker::~WindowsSpellChecker()
{
	delete d;
}

SpellChecker::Result WindowsSpellChecker::check(WordLangTuple const &wl)
{
	LYXERR0("Want to check " << wl.word() << "!");
	ISpellChecker *speller = d->getOrCreate(wl.lang());
	if (speller == nullptr) {
		LYXERR0("No dictionary found.");
		return NO_DICTIONARY;
	}

	LYXERR(Debug::GUI,
	       "spellCheck: \"" << wl.word() << "\", lang = " << wl.lang()->lang());

	std::wstring word = to_utf16(wl.word());
	wprintf(L" -> Windows word check: %s\n", word.c_str());

	IEnumSpellingError *enumSpellingError = nullptr;
	HRESULT hr = speller->Check(word.c_str(), &enumSpellingError);
	if (SUCCEEDED(hr)) {
		hr = S_OK;
		size_t numErrors = 0;
		while (S_OK == hr) {
			ISpellingError *spellingError = nullptr;
			hr = enumSpellingError->Next(&spellingError);
			if (S_OK == hr) {
				++numErrors;
			}
			if (spellingError != nullptr) {
				spellingError->Release();
			}
		}

		enumSpellingError->Release();

		if (numErrors == 0) {
			return WORD_OK;
		} else {
			return UNKNOWN_WORD;
		}
	} else {
		if (enumSpellingError != nullptr) {
			enumSpellingError->Release();
		}
		return NO_DICTIONARY;
	}
}

void WindowsSpellChecker::suggest(WordLangTuple const &wl, docstring_list &suggestions)
{
	LYXERR0("Want to suggest " << wl.word() << "!");
	ISpellChecker *speller = d->getOrCreate(wl.lang());
	if (speller == nullptr) {
		return;
	}

	std::wstring word = to_utf16(wl.word());
	wprintf(L" -> Windows word suggest: %s\n", word.c_str());

	IEnumString *enumSuggestions = nullptr;
	HRESULT hr = speller->Suggest(word.c_str(), &enumSuggestions);
	if (SUCCEEDED(hr)) {
		hr = S_OK;
		while (S_OK == hr) {
			LPOLESTR winSuggestion = nullptr;
			hr = enumSuggestions->Next(1, &winSuggestion, nullptr);
			if (S_OK == hr) {
				std::wstring wideSuggestion = std::wstring(winSuggestion);
				docstring suggestion = from_utf16(wideSuggestion);
				wprintf(L" -> Windows suggestion: %s\n", winSuggestion);
				LYXERR0(" -> Windows UCS4 suggest " << suggestion);
				suggestions.push_back(suggestion);
				CoTaskMemFree(winSuggestion);
			}
		}
	}
	if (enumSuggestions != nullptr) {
		enumSuggestions->Release();
	}
}

void WindowsSpellChecker::insert(WordLangTuple const &wl)
{
	LYXERR(Debug::GUI, "learn word: \"" << wl.word() << "\"");
	ISpellChecker *speller = d->getOrCreate(wl.lang());
	if (speller == nullptr) {
		return;
	}

	std::wstring word = to_utf16(wl.word());
	wprintf(L" -> Windows word add: %s\n", word.c_str());

	speller->Add(word.c_str());
	advanceChangeNumber();
}

void WindowsSpellChecker::remove(WordLangTuple const &wl)
{
	LYXERR(Debug::GUI, "unlearn word: \"" << wl.word() << "\"");
	ISpellChecker *speller = d->getOrCreate(wl.lang());
	if (speller == nullptr) {
		return;
	}

	// Remove is only possible sometimes.
	ISpellChecker2 *betterSpeller = nullptr;
	HRESULT hr = speller->QueryInterface(__uuidof(ISpellChecker2),
	                                     reinterpret_cast<void **>(&betterSpeller));
	if (SUCCEEDED(hr)) {
		std::wstring word = to_utf16(wl.word());
		wprintf(L" -> Windows word remove: %s\n", word.c_str());
		betterSpeller->Remove(word.c_str());
		advanceChangeNumber();
	}
}

void WindowsSpellChecker::accept(WordLangTuple const &wl)
{
	LYXERR0("Ignoring a word.");
	LYXERR(Debug::GUI, "ignore word: \"" << wl.word() << "\"");
	ISpellChecker *speller = d->getOrCreate(wl.lang());
	if (speller == nullptr) {
		return;
	}

	std::wstring word = to_utf16(wl.word());
	wprintf(L" -> Windows word accept: %s\n", word.c_str());
	speller->Ignore(word.c_str());
	advanceChangeNumber();
}

bool WindowsSpellChecker::hasDictionary(Language const *lang) const
{
	std::wstring tag = langToBcp47(lang);

	BOOL isSupported = FALSE;
	const HRESULT hr = d->factory->IsSupported(tag.c_str(), &isSupported);
	wprintf(L" Checking dictionary %s -> %d\n", tag.c_str(), isSupported);
	return SUCCEEDED(hr) && isSupported;
}

int WindowsSpellChecker::numDictionaries() const
{
	LYXERR0("Number of dicts requested");
	return count_if(d->spellers.begin(), d->spellers.end(),
	                [](auto speller) { return speller.second != nullptr; });
}

docstring const WindowsSpellChecker::error() // NOLINT(readability-const-return-type)
{
	return docstring();
}

void WindowsSpellChecker::advanceChangeNumber()
{
	nextChangeNumber();
}

std::wstring langToBcp47(Language const *lang)
{
	// This is taken from HunspellChecker.cpp
	std::string langId = lang->variety().empty()
	                     ? lang->code()
	                     : lang->code() + "-" + lang->variety();
	// Sometimes, there are underscores in the language names, which we don't want.
	replace(langId.begin(), langId.end(), '_', '-');
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
	return converter.from_bytes(langId);
}

std::wstring to_utf16(const docstring &source)
{
	// On Windows, wchar_t is char16_t, same size as unsigned short.
//	static_assert(sizeof(wchar_t) == sizeof(unsigned short), "Wrong sizes.");
//	std::vector<unsigned short> utf16 = ucs4_to_utf16(source.c_str(), source.size());
//	return std::wstring(utf16.begin(), utf16.end());
	std::string utf8 = to_utf8(source);
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
	return converter.from_bytes(utf8);
}

docstring from_utf16(const std::wstring &source)
{
	// On Windows, wchar_t is char16_t, same size as unsigned short.
//	static_assert(sizeof(wchar_t) == sizeof(unsigned short), "Wrong sizes.");
//	const wchar_t* underlying = source.c_str();
//	auto casted = reinterpret_cast<const unsigned short*>(underlying);
//	std::vector<char_type> result = utf16_to_ucs4(casted, source.size());
//	return docstring(result.begin(), result.end());
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
	std::string utf8 = converter.to_bytes(source);
	return from_utf8(utf8);
}

} // namespace lyx
