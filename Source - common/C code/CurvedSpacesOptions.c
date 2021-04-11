//	CurvedSpacesOptions.c
//
//	© 2021 by Jeff Weeks
//	See TermsOfUse.txt

#include "CurvedSpaces-Common.h"


void SetCenterpiece(
	ModelData		*md,
	CenterpieceType	aCenterpieceChoice)
{
	md->itsCenterpieceType = aCenterpieceChoice;
	md->itsChangeCount++;
}

void SetShowObserver(
	ModelData	*md,
	bool		aShowObserverChoice)
{
	md->itsShowObserver = aShowObserverChoice;
	md->itsChangeCount++;
}

void SetShowColorCoding(
	ModelData	*md,
	bool		aShowColorCodingChoice)
{
	md->itsShowColorCoding = aShowColorCodingChoice;
	md->itsDirichletWallsMeshNeedsRefresh = true;	//	The color is written into the mesh itself, so rebuild it.
	md->itsChangeCount++;
}

void SetShowCliffordParallels(
	ModelData		*md,
	CliffordMode	aCliffordMode)
{
	md->itsCliffordMode = aCliffordMode;
	md->itsChangeCount++;
}

void SetShowVertexFigures(
	ModelData	*md,
	bool		aShowVertexFiguresChoice)
{
	md->itsShowVertexFigures = aShowVertexFiguresChoice;
	md->itsChangeCount++;
}

void SetFogFlag(
	ModelData	*md,
	bool		aFogFlag)
{
	md->itsFogFlag = aFogFlag;
	md->itsChangeCount++;
}

#ifdef HANTZSCHE_WENDT_AXES
void SetShowHantzscheWendtAxes(
	ModelData	*md,
	bool		aShowHantzscheWendtAxesChoice)
{
	md->itsShowHantzscheWendtAxes = aShowHantzscheWendtAxesChoice;
	md->itsChangeCount++;
}
#endif
