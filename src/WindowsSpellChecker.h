// -*- C++ -*-
/**
 * \file WindowsSpellChecker.h
 * This file is part of LyX, the document processor.
 * License details can be found in the file COPYING.
 *
 * \author Niko Strijbol
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef LYX_WINDOWSSPELL_H
#define LYX_WINDOWSSPELL_H

#include "SpellChecker.h"

namespace lyx {

/**
 * Provides integration with the native spell checker on Windows, which is available on
 * Windows 8+.
 *
 * \see https://docs.microsoft.com/en-us/windows/win32/intl/spell-checker-api
 */
class WindowsSpellChecker : public SpellChecker
{
public:
	WindowsSpellChecker();
	~WindowsSpellChecker() override;

	/// \name SpellChecker inherited methods
	//@{
	enum Result check(WordLangTuple const &) override;
	void suggest(WordLangTuple const &, docstring_list &) override;
	void stem(WordLangTuple const &, docstring_list &) override {};
	void insert(WordLangTuple const &) override;
	void remove(WordLangTuple const &) override;
	void accept(WordLangTuple const &) override;
	bool hasDictionary(Language const * lang) const override;
	int numDictionaries() const override;
	docstring const error() override;
	void advanceChangeNumber() override;
	//@}
private:
	struct Private;
	Private * d;
};


} // namespace lyx

#endif // LYX_WINDOWSSPELL_H
