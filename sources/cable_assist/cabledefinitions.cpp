#include "cabledefinitions.h"
#include "qlist.h"


QStringList QETCableStandard::cableType()
{
	QStringList type_list{
		"",
		"A2Y(L)2Y",
		"H07RN-F",
		"J-H(St)H",
		"J-Y(St)Y",
		"JE-Y(ST)Y",
		"Cat5",
		"Cat6",
		"Cat7",
		"LiYY",
		"LiYCY",
		"LiYCY_pair",
		"NAYY",
		"NYCY",
		"NYCWY",
		"NAYCWY",
		"NHXMH",
		"NYM",
		"(N)YM-(St)",
		"NYY",
		"Oelflex_Classic_100",
		"YSLY-JB",
		};
}

QStringList QETCableStandard::condCrossSectionUnit(int unit)
{
	QStringList unit_list;
	switch (unit){
		case QETCableStandard::mm:
			unit_list+={"","0,6","0,8"};
			break;

		case QETCableStandard::qmm:
			unit_list+={"","0,14","0,25","0,5","0,75","1","1,5","2,5","4","6","10","16","25","35",
						"50","70","95","120","150","185","240","300","400","500","630","800","1000",};
			break;

		case QETCableStandard::AWG:
			unit_list+={"","32","30","28","26","24","22","20","18","16","14","12","10","8",
						 "6","4","2","1","0","00","000","0000"};
			break;
		default:
			break;
	}
	return(unit_list);
}

QList<QList<QString>> QETCableStandard::condIdentificationStandards()
{
	QList<QList<QString>>condIdentificationStandard = {{"",""},
													{"DIN47100",
													 "Electronic control cables \n "
													 "Elektronik Steuerleitungen \n "
													 "LIYCY, LIYY (ws,bn,gn,gegr,rs,bl,rt,sw,vio)"},
													{"Numbers",
													 " \n"
													 "Die Adern sind nummeriert \n"
													 ""},
													{"VDE0293_308",											// "VDE0293_308_S2"
													 "Multicore cables, actual standard \n"
													 "Mehradrige Kabel und Leitungen, aktuelle Norm \n"
													 "NYM, NYY, NAYY, H07RN-F (bn, sw, gr, bl, gnge)"},
													{"VDE0293_308_old_massive",								// "VDE0293_308_S1_massive"
													 "Massive multicore cables old standard \n"
													 "Massive mehradrige Kabel alte Norm \n"
													 "NYM, NYY, NAYY, (sw, bn, sw, bl, gnge)"},
													{"VDE0293_308_old_flex",								// "VDE0293_308_S1_flex"
													 "Flexible multicore cables old standard \n"
													 "Flexible mehradrige Leitungen alte Norm \n"
													 "H07RN-F (sw, bn, sw, bl, gnge)"}, // prüfen ob H07RN-F als Beispiel richtig ist
													{"VDE0293_color",
													 "PVC control cables with more than 6 colored wires\n"
													 "PVC Steuerleitung mit mehr wie 6 farbigen Adern \n"
													 "YSLY-JB, (gnge, ws, sw, bl, bn, gr, rt, ...)"},
													{"VDE0815_2x2",
													 ""},
													{"VDE0815_from_4x2",
													 ""},
													{"VDE0816_star_quad_ab",
													 ""},
													{"VDE0816_star_quad_color_ab",
													 ""},
													{"VDE0816_star_quad_ring",
														 ""}};
	return(condIdentificationStandard);
}

