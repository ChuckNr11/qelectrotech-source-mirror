/*
	Copyright 2006-2026 The QElectroTech Team
	This file is part of QElectroTech.
	https://www.qelectrotech.org/
	See the file COPYING file, or this file's source file, for details of the
	license.
*/
/**
	@file terminalsymbollist.h
	@brief Header of the TerminalSymbolList class
 */
#ifndef TERMINALSYMBOLLIST_H
#define TERMINALSYMBOLLIST_H

#include "../../ElementsCollection/elementslocation.h"

#include <QFutureWatcher>
#include <QHash>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>

class QETProject;

/**
	@brief The TerminalSymbolList class

	Every symbol a terminal can be drawn with, read once for the dialogs of
	the terminal navigator and asked for by name as long as those dialogs
	are open.

	Two sources make up that list, and they are worth keeping apart: the
	embedded collection of the project, which lives in memory and can be
	read the moment this class is built, and the standard collection, which
	holds more than a thousand elements on disk and is therefore read in the
	background (@sa startCommonCollectionScan) -- reading those files on the
	GUI thread would freeze the dialog which asked for them. Whoever wants
	the full list starts the scan and listens to symbolsRead().

	The list exists for two dialogs which need the very same symbols: the
	dialog of a terminal strip gives its symbol combo box the list itself
	(@sa TerminalPropertiesDialog), while the dialog which describes the
	strip only has to name the symbols its preview tree shows, and never
	shows a list of them (@sa AddTerminalsDialog::symbolName). Both are
	handed the same instance by the strip dialog, so that the names one of
	them reads are known to the other one as well.

	A symbol is named by its id-free location string ('embed://...' or
	'common://...' -- never the runtime project id of one session, which
	means nothing in the next one, @sa RealTerminal::symbolWithoutProjectId).
*/
class TerminalSymbolList : public QObject
{
	Q_OBJECT

	public:
		explicit TerminalSymbolList(QETProject *project, QObject *parent = nullptr);
		~TerminalSymbolList() override;

		/**
			@brief startCommonCollectionScan
			Reads the terminal symbols of the standard collection in the
			background and appends them to the list: symbolsRead() is
			emitted when they are in. Called as often as needed -- the
			second call finds the scan done or running and does nothing.
		*/
		void startCommonCollectionScan();
		/// @return true once the symbols of the standard collection are in.
		bool isScanned() const {return m_scanned;}
		/// @return true while those symbols are being read in the background.
		bool isScanning() const {return m_scanning;}

		/// @return the symbols read so far, the ones of the project first,
		/// in the order they should be listed in.
		const QList<ElementsLocation> &symbols() const {return m_symbols;}

		/// @return true when @a symbol is listed already (@sa symbols).
		bool holds(const QString &symbol) const {return m_paths.contains(symbol);}
		/**
			@brief name
			@param symbol : the id-free location string of a symbol
			@return the name to show for it, read once and then kept: an
			element name is read out of its XML, which is not worth doing
			twice. A symbol which is not listed (not read yet, or left over
			from a collection which does not answer for it anymore) is
			resolved through the project the way the terminals do it
			(@sa RealTerminal::symbolLocation) -- such a name is only kept
			while it says something, so that a symbol listed later on is
			named from its list after all.
		*/
		QString name(const QString &symbol);

	signals:
		/// Emitted when symbols were added to the list.
		void symbolsRead();

	private:
		void readProjectSymbols();
		void readTerminalSymbols();
		void append(const ElementsLocation &location);
		void onScanFinished();

		QETProject *m_project;
		QList<ElementsLocation> m_symbols;
		QSet<QString> m_paths;
		QHash<QString, QString> m_names;
		/// Fills the symbols of the standard collection, in the background.
		QFutureWatcher<QStringList> *m_watcher{nullptr};
		bool m_scanning{false};
		bool m_scanned{false};
};

#endif // TERMINALSYMBOLLIST_H
