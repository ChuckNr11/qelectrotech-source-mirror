/*
	Copyright 2006-2026 The QElectroTech Team
	This file is part of QElectroTech.

	QElectroTech is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	QElectroTech is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with QElectroTech.  If not, see <http://www.gnu.org/licenses/>.
*/
/*
	The symbol editor's wheel zoom, as the folio's (tst_zoomstep.cpp for
	DiagramView): one step per notch (120) of the wheel, small turns of a
	smooth wheel add up, no zoom while a mouse button is held (the middle
	button is the wheel itself, pressing it to pan must not zoom), and a
	sideways turn does not zoom.
*/
#include "qetapptest.h"
#include "editor/elementview.h"
#include "editor/ui/qetelementeditor.h"

#include <QWheelEvent>

class tst_elementviewzoom : public QObject
{
	Q_OBJECT

	private:
		QETElementEditor *m_editor = nullptr;
		ElementView *m_view = nullptr;

		qreal percent() const { return m_view->transform().m11() * 100; }

		void setPercent(qreal p)
		{
			m_view->zoom(p / percent());
		}

		void wheel(QPoint angle, Qt::KeyboardModifiers modifiers = Qt::NoModifier,
				   Qt::MouseButtons buttons = Qt::NoButton)
		{
			QWheelEvent event(QPointF(50, 50),
							  m_view->viewport()->mapToGlobal(QPointF(50, 50)),
							  QPoint(), angle, buttons, modifiers,
							  Qt::NoScrollPhase, false);
			QApplication::sendEvent(m_view->viewport(), &event);
		}

	private slots:
		void initTestCase()
		{
			m_editor = new QETElementEditor();
			m_editor->resize(1000, 700);
			QCoreApplication::processEvents();
			m_view = m_editor->elementView();
			QVERIFY(m_view);
		}

		void cleanupTestCase()
		{
			DiagramView::setZoomStep(0);
			delete m_editor;
		}

			//One step per notch; a smooth wheel's small turns add up.
			//Slots run in this order and every turn below adds up to a
			//whole notch, so the accumulator starts and ends empty.
		void oneStepPerNotch()
		{
			DiagramView::setZoomStep(5);
			setPercent(100);
			wheel(QPoint(0, 120));
			QVERIFY2(qAbs(percent() - 105) < 1e-6, qPrintable(QString::number(percent())));
			wheel(QPoint(0, 60));
			QVERIFY2(qAbs(percent() - 105) < 1e-6, qPrintable(QString::number(percent())));
			wheel(QPoint(0, 60));
			QVERIFY2(qAbs(percent() - 110) < 1e-6, qPrintable(QString::number(percent())));
			wheel(QPoint(0, -240));
			QVERIFY2(qAbs(percent() - 100) < 1e-6, qPrintable(QString::number(percent())));
		}

			//Alt with the wheel: four steps per notch
		void altWheel()
		{
			DiagramView::setZoomStep(5);
			setPercent(100);
			wheel(QPoint(0, 120), Qt::AltModifier);
			QVERIFY2(qAbs(percent() - 120) < 1e-6, qPrintable(QString::number(percent())));
			wheel(QPoint(0, -120), Qt::AltModifier);
			QVERIFY2(qAbs(percent() - 100) < 1e-6, qPrintable(QString::number(percent())));
		}

			//No zoom while a mouse button is held: the middle button is
			//the wheel, pressing it to pan jogs the wheel. The jog must
			//not zoom and must not eat into the next notch either.
		void noZoomWhileButtonHeld()
		{
			DiagramView::setZoomStep(5);
			setPercent(100);
			wheel(QPoint(0, 120), Qt::NoModifier, Qt::MiddleButton);
			wheel(QPoint(0, 120), Qt::NoModifier, Qt::LeftButton);
			wheel(QPoint(0, 60), Qt::NoModifier, Qt::MiddleButton);
			QVERIFY2(qAbs(percent() - 100) < 1e-6, qPrintable(QString::number(percent())));
			wheel(QPoint(0, 60));
			QVERIFY2(qAbs(percent() - 100) < 1e-6, qPrintable(QString::number(percent())));
			wheel(QPoint(0, 60));
			QVERIFY2(qAbs(percent() - 105) < 1e-6, qPrintable(QString::number(percent())));
			wheel(QPoint(0, -120));
			QVERIFY2(qAbs(percent() - 100) < 1e-6, qPrintable(QString::number(percent())));
		}

			//A sideways turn does not zoom (it was a zoom out once: the
			//angle was tested for being greater than zero)
		void sidewaysTurnDoesNotZoom()
		{
			DiagramView::setZoomStep(5);
			setPercent(100);
			wheel(QPoint(120, 0));
			QVERIFY2(qAbs(percent() - 100) < 1e-6, qPrintable(QString::number(percent())));
			DiagramView::setZoomStep(0);
			wheel(QPoint(120, 0));
			QVERIFY2(qAbs(percent() - 100) < 1e-6, qPrintable(QString::number(percent())));
		}

			//Zoom step setting off, as the folio: a smooth zoom by the
			//angle, so small turns stay small
		void smoothZoomWithoutStep()
		{
			DiagramView::setZoomStep(0);
			setPercent(100);
			qreal before = percent();
			wheel(QPoint(0, 30));
			QVERIFY2(qAbs(percent() - before * 1.03) < 1e-6, qPrintable(QString::number(percent())));
			before = percent();
			wheel(QPoint(0, 90));
			QVERIFY2(qAbs(percent() - before * 1.09) < 1e-6, qPrintable(QString::number(percent())));
		}
};

QET_APP_TEST_MAIN(tst_elementviewzoom)
#include "tst_elementviewzoom.moc"
