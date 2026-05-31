#ifndef CABLEDEFINITIONS_H
#define CABLEDEFINITIONS_H

#include "qregexp.h"
namespace QETCableStandard
{
	enum CrossSectionUnit{
		mm  =1,
		qmm =2,
		AWG =3
	};

	QStringList cableType();
	QStringList condCrossSectionUnit(int);
	QList<QList<QString>> condIdentificationStandards();
}

#endif // CABLEDEFINITIONS_H
