/*
	Copyright 2006-2026 The QElectroTech Team
	This file is part of QElectroTech.
	https://www.qelectrotech.org/
	See the file COPYING file, or this file's source file, for details of the
	license.
*/
/**
	@file terminalsymbollist.cpp
	@brief Implementation of the TerminalSymbolList class
 */
#include "terminalsymbollist.h"

#include "../../ElementsCollection/xmlelementcollection.h"
#include "../../TerminalStrip/realterminal.h"
#include "../../TerminalStrip/terminalstrip.h"
#include "../../elementprovider.h"
#include "../../qetapp.h"
#include "../../qetgraphicsitem/element.h"
#include "../../qetproject.h"
#include "../../utils/qetutils.h"

#include <QDir>
#include <QDirIterator>
#include <QDomDocument>
#include <QFile>
#include <QtConcurrentRun>

#include <algorithm>

namespace {
	/// Reads the root of an element file and tells whether it is a terminal
	/// symbol. Used by the background scan of the standard collection: a
	/// plain file read, no collection, no project, no shared state, so it
	/// can run off the GUI thread.
	bool fileIsATerminalSymbol(const QString &file_path)
	{
		QFile file(file_path);
		if (!file.open(QIODevice::ReadOnly)) {
			return false;
		}

		QDomDocument document;
		if (!document.setContent(&file)) {
			return false;
		}

		return document.documentElement().attribute(QStringLiteral("link_type"))
				== QLatin1String("terminal");
	}
}

/**
	@brief TerminalSymbolList::TerminalSymbolList
	@param project : the project whose symbols are read right away -- its
	embedded collection lives in memory, reading it costs nothing worth
	mentioning, and every dialog of the navigator wants those symbols
	(@sa readProjectSymbols).
	@param parent
	The standard collection is read on demand only
	(@sa startCommonCollectionScan), since it reads more than a thousand
	files from disk.
*/
TerminalSymbolList::TerminalSymbolList(QETProject *project, QObject *parent) :
	QObject(parent),
	m_project(project)
{
	readProjectSymbols();
	readTerminalSymbols();
}

TerminalSymbolList::~TerminalSymbolList()
{
	if (m_watcher) {
			//A scan still running would emit into an object which is
			//going away: the watcher is this object's child and would
			//be destroyed with it, but the future it watches is not
			//cancelled by that -- wait for it instead of leaving the
			//pool with a job nobody owns.
		m_watcher->waitForFinished();
	}
}

/**
	@brief TerminalSymbolList::readProjectSymbols
	Adds every terminal symbol of the project's own collection to the list.
	Those symbols are the ones the project drew its terminals with, so they
	are listed ahead of the standard collection's ones -- and they are the
	only ones a brand new project has.
*/
void TerminalSymbolList::readProjectSymbols()
{
	if (!m_project || !m_project->embeddedElementCollection()) {
		return;
	}

	const auto definitions = m_project->embeddedElementCollection()->root()
		.elementsByTagName(QStringLiteral("definition"));
	for (int i = 0 ; i < definitions.count() ; ++i) {
		const auto definition = definitions.at(i).toElement();
		if (definition.attribute(QStringLiteral("link_type")) != QLatin1String("terminal")) {
			continue;
		}
		append(m_project->embeddedElementCollection()->domToLocation(
			definition.parentNode().toElement()));
	}
}

/**
 * @brief TerminalSymbolList::readTerminalSymbols
 * Adds the symbols the terminals of the project are drawn with to the
 * list, whatever collection they come from. A terminal keeps the symbol
 * it was created with even when no collection answers for it anymore,
 * and a list which lost it would name nothing for the rows which show it
 * (@sa name) -- while the user reads it in the row of symbols and can
 * give it to the next terminals as well. Every terminal counts here:
 * the ones drawn on a folio, the ones waiting for their symbol and the
 * independent ones, which is the very list the navigator shows.
 */
void TerminalSymbolList::readTerminalSymbols()
{
	if (!m_project) {
		return;
	}

	const auto add = [this](const QString &symbol) {
		const auto id_free = RealTerminal::symbolWithoutProjectId(symbol);
		if (id_free.isEmpty()) {
			return;
		}
			//Only 'embed://...' needs the project to resolve, and takes
			//it for an embedded one when it is given for any other
			//@sa RealTerminal::symbolLocation.
		append(RealTerminal::symbolLocation(id_free, m_project));
	};

	for (const auto &strip : m_project->terminalStrip()) {
		for (const auto &real : strip->realTerminals()) {
			add(real->isElement() && real->element()
				? real->element()->location().collectionPath()
				: real->symbol());
		}
	}

	ElementProvider provider(m_project);
	for (auto *terminal : provider.freeTerminal()) {
		add(terminal->location().collectionPath());
	}
}

/**
	@brief TerminalSymbolList::startCommonCollectionScan
	Reads the terminal symbols of the standard collection in the background
	and appends them to the list once they are found. Reading eleven hundred
	files on the GUI thread would freeze whatever asked for them, which is
	why this is a future the caller listens for instead of a plain call
	(@sa symbolsRead).
	The scan is started once: a second dialog asking for it finds it done or
	running, and its symbols are in the list either way.
*/
void TerminalSymbolList::startCommonCollectionScan()
{
	if (m_scanning || m_scanned) {
		return;
	}
	m_scanning = true;

	if (!m_watcher) {
		m_watcher = new QFutureWatcher<QStringList>(this);
		connect(m_watcher, &QFutureWatcher<QStringList>::finished,
			this, &TerminalSymbolList::onScanFinished);
	}

	const auto common_dir = QETApp::commonElementsDirN();
	m_watcher->setFuture(QtConcurrent::run([common_dir]() {
		QStringList paths;
		QDirIterator it(common_dir,
						QStringList{QStringLiteral("*.elmt")},
						QDir::Files,
						QDirIterator::Subdirectories);
		while (it.hasNext()) {
			const auto path = it.next();
			if (fileIsATerminalSymbol(path)) {
				paths << path;
			}
		}
		std::sort(paths.begin(), paths.end(), QETUtils::naturalLessThan);
		return paths;
	}));
}

/**
	@brief TerminalSymbolList::onScanFinished
	Adds the symbols found in the standard collection to the list and tells
	whoever lists them that they are there now.
*/
void TerminalSymbolList::onScanFinished()
{
	m_scanning = false;
	m_scanned = true;

	const auto common_dir = QETApp::commonElementsDirN();
	for (const auto &path : m_watcher->result()) {
		auto relative_path = QDir(common_dir).relativeFilePath(path);
		relative_path.replace(QLatin1Char('\\'), QLatin1Char('/'));
		append(ElementsLocation(QStringLiteral("common://") + relative_path));
	}

	emit symbolsRead();
}

/**
	@brief TerminalSymbolList::append
	Adds @a symbol to the list, unless a symbol of that very location is
	listed already -- the same element may well be part of the project and
	of the standard collection at once, and it would be listed twice.

	Only the location is kept: the name of an element is read out of its
	XML, which is what the dialogs do when they show it, and what name()
	does when somebody asks for one without showing the list
	(@sa the cache of m_names).
*/
void TerminalSymbolList::append(const ElementsLocation &location)
{
	const auto location_string = location.collectionPath();
	if (location_string.isEmpty() || m_paths.contains(location_string)) {
		return;
	}

	m_paths.insert(location_string);
	m_symbols << location;
}

/**
	@brief TerminalSymbolList::name
	@param symbol : the id-free location string of a symbol
	@return the name to show for it (@sa TerminalSymbolList).
*/
QString TerminalSymbolList::name(const QString &symbol)
{
	if (symbol.isEmpty()) {
		return QString();
	}

	const auto known = m_names.constFind(symbol);
	if (known != m_names.constEnd() && !known->isEmpty()) {
		return *known;
	}

		//Listed already: its name is the one to show, which is the whole
		//point of the list -- an element name is read out of its XML, and
		//reading it once only is what the cache above is for
	for (const auto &location : m_symbols) {
		if (location.collectionPath() == symbol) {
			const auto listed = location.name();
			if (!listed.isEmpty()) {
				m_names.insert(symbol, listed);
			}
			return listed;
		}
	}

		//Not listed: the symbol may be one the list has not read yet, or
		//one of a collection which does not answer for it anymore. The
		//project resolves an id-free symbol of its own collection; a
		//'common://...' one does not need it and must not be given it, or
		//it would be taken for an embedded path
		//(@sa RealTerminal::symbolLocation). Nothing is kept for an empty
		//name, so that a symbol listed later on is named from its list
		//after all (@sa symbolsRead).
	const auto read = RealTerminal::symbolLocation(symbol, m_project).name();
	if (!read.isEmpty()) {
		m_names.insert(symbol, read);
	}

	return read;
}
