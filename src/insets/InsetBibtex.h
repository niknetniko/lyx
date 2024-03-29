// -*- C++ -*-
/**
 * \file InsetBibtex.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Alejandro Aguilar Sierra
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef INSET_BIBTEX_H
#define INSET_BIBTEX_H

#include "InsetCommand.h"

namespace lyx {

class BiblioInfo;
class docstring_list;

namespace support {
	class FileName;
} // namespace support

/** Used to insert BibTeX's information
  */
class InsetBibtex : public InsetCommand {
public:
	///
	InsetBibtex(Buffer *, InsetCommandParams const &);

	///
	docstring_list getBibFiles() const;
	///
	bool addDatabase(docstring const &);
	///
	bool delDatabase(docstring const &);
	///
	void write(std::ostream &) const;

	/// \name Public functions inherited from Inset class
	//@{
	///
	docstring toolTip(BufferView const & bv, int x, int y) const;
	///
	bool hasSettings() const { return true; }
	///
	InsetCode lyxCode() const { return BIBTEX_CODE; }
	///
	DisplayType display() const { return AlignCenter; }
	///
	void latex(otexstream &, OutputParams const &) const;
	///
	int plaintext(odocstringstream & ods, OutputParams const & op,
	              size_t max_length = INT_MAX) const;
	///
	void updateBuffer(ParIterator const &, UpdateType);
	///
	void addToToc(DocIterator const & di, bool output_active,
				  UpdateType utype, TocBackend & backend) const;
	///
	void collectBibKeys(InsetIterator const &, support::FileNameList &) const;
	///
	void validate(LaTeXFeatures &) const;
	///
	docstring xhtml(XHTMLStream &, OutputParams const &) const;
	///
	std::string contextMenuName() const;
	//@}

	/// \name Static public methods obligated for InsetCommand derived classes
	//@{
	///
	static ParamInfo const & findInfo(std::string const &);
	///
	static std::string defaultCommand() { return "bibtex"; }
	///
	static bool isCompatibleCommand(std::string const & s)
		{ return s == "bibtex"; }
	//@}

private:
	///
	void editDatabases(docstring const db = docstring()) const;
	///
	void parseBibTeXFiles(support::FileNameList &) const;
	///
	bool usingBiblatex() const;
	///
	docstring getRefLabel() const;
	///
	std::map<std::string, std::string> getFileEncodings() const;

	/// \name Private functions inherited from Inset class
	//@{
	///
	bool getStatus(Cursor & cur, FuncRequest const & cmd,
		FuncStatus & flag) const;
	///
	void doDispatch(Cursor & cur, FuncRequest & cmd);
	///
	Inset * clone() const { return new InsetBibtex(*this); }
	//@}

	/// \name Private functions inherited from InsetCommand class
	//@{
	///
	docstring screenLabel() const;
	//@}
};


} // namespace lyx

#endif // INSET_BIBTEX_H
