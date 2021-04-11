//	CurvedSpacesFileIO.c
//
//	Accept files in either UTF-8 or Latin-1,
//	subject to the condition that non-ASCII characters
//	may appear only in comments.  In other words, assume
//	the matrix entries are written using plain 7-bit ASCII only.
//	If using UTF-8, allow but do not require a byte-order-mark.
//
//	© 2021 by Jeff Weeks
//	See TermsOfUse.txt

#include "CurvedSpaces-Common.h"
#if (defined(HIGH_RESOLUTION_SCREENSHOT) || defined(SHAPE_OF_SPACE_CH_15) || defined(SHAPE_OF_SPACE_CH_16))
#include <math.h>
#endif


#define HYPERBOLIC_TILING_RADIUS_PADDING	1.0


//	A quick-and-dirty hack tiles the mirrored dodecahedron
//	and the Seifert-Weber space (which have relatively large volumes)
//	more deeply than the smaller-volume hyperbolic spaces.
//	A more robust algorithm would examine the size
//	of the fundamental domain.
typedef enum
{
	HyperbolicSpaceGeneric,
	HyperbolicSpaceMirroredDodecahedron,
	HyperbolicSpaceSeifertWeber
} HyperbolicSpaceType;


static bool			StringBeginsWith(Byte *anInputText, Byte *aPossibleBeginning);
static void			RemoveComments(Byte *anInputText);
static ErrorText	ReadMatrices(Byte *anInputText, MatrixList **aMatrixList);
static bool			ReadOneNumber(Byte *aString, double *aValue, Byte **aStoppingPoint, ErrorText *anError);
static ErrorText	LoadGenerators(ModelData *md, MatrixList *aGeneratorList, HyperbolicSpaceType aHyperbolicSpaceType);
static ErrorText	DetectSpaceType(MatrixList *aGeneratorList, SpaceType *aSpaceType);


ErrorText LoadGeneratorFile(
	ModelData	*md,
	Byte		*anInputText)	//	zero-terminated, and hopefully UTF-8 or Latin-1
{
	ErrorText			theErrorMessage	= NULL;
	HyperbolicSpaceType	theHyperbolicSpaceType;
	MatrixList			*theGenerators	= NULL;

	//	Make sure we didn't get UTF-16 data by mistake.
	if ((anInputText[0] == 0xFF && anInputText[1] == 0xFE)
	 || (anInputText[0] == 0xFE && anInputText[1] == 0xFF))
	{
		theErrorMessage = u"The matrix file is in UTF-16 format.  Please convert to UTF-8.";
		goto CleanUpLoadGeneratorFile;
	}
	
	//	If a UTF-8 byte-order-mark is present, skip over it.
	if (anInputText[0] == 0xEF && anInputText[1] == 0xBB && anInputText[2] == 0xBF)
		anInputText += 3;

	//	As special cases, check whether anInputText begins with
	//
	//		#	Mirrored Right-Angled Dodecahedron
	//	or
	//		#	Seifert-Weber Dodecahedral Space
	//
	if (StringBeginsWith(anInputText, (Byte *)"#	Mirrored Right-Angled Dodecahedron"))
		theHyperbolicSpaceType = HyperbolicSpaceMirroredDodecahedron;
	else
	if (StringBeginsWith(anInputText, (Byte *)"#	Seifert-Weber Dodecahedral Space"))
		theHyperbolicSpaceType = HyperbolicSpaceSeifertWeber;
	else
		theHyperbolicSpaceType = HyperbolicSpaceGeneric;

	//	Remove comments.
	//	What remains should be plain 7-bit ASCII (common to both UTF-8 and Latin-1).
	RemoveComments(anInputText);

	//	Parse the input text into 4×4 matrices.
	theErrorMessage = ReadMatrices(anInputText, &theGenerators);
	if (theErrorMessage != NULL)
		goto CleanUpLoadGeneratorFile;
		
	//	Load theGenerators.
	theErrorMessage = LoadGenerators(md, theGenerators, theHyperbolicSpaceType);
	if (theErrorMessage != NULL)
		goto CleanUpLoadGeneratorFile;

CleanUpLoadGeneratorFile:

	FreeMatrixList(&theGenerators);

	return theErrorMessage;
}

static bool StringBeginsWith(
	Byte	*anInputText,			//	zero-terminated, UTF-8 or Latin-1
	Byte	*aPossibleBeginning)	//	zero-terminated, UTF-8 or Latin-1
{
	Byte	*a,
			*b;
	
	a = anInputText;
	b = aPossibleBeginning;
	
	while (*b != 0)
	{
		if (*a++ != *b++)
			return false;
	}
	
	return true;
}

static void RemoveComments(
	Byte *anInputText)	//	Zero-terminated input string also serves for output.
{
	//	Remove comments in place.
	//	A comment begins with a '#' character and runs to the end of the line,
	//	which may be marked by '\r' or '\n' or both.
	//	The input text must be zero-terminated.
	//	Both UTF-8 and Latin-1 are acceptable.

	Byte	*r,	//	read  location
			*w;	//	write location

	r = anInputText;
	w = anInputText;

	do
	{
		if (*r == '#')
		{
			do
			{
				r++;
			} while (*r != '\r' && *r != '\n' && *r != 0);
		}
	} while ((*w++ = *r++) != 0);
}


static ErrorText ReadMatrices(
	Byte			*anInputText,	//	zero-terminated comment-free input string
	MatrixList		**aMatrixList)
{
	ErrorText		theErrorMessage		= NULL;
	unsigned int	theNumNumbers		= 0,
					theNumGenerators	= 0;
	Byte			*theMarker			= NULL;
	unsigned int	i,
					j,
					k;

	//	Check the input parameters.
	if (*aMatrixList != NULL)
		return u"ReadMatrices() was passed a non-NULL output pointer.";

	//	Count the number of numbers in anInputText.
	theNumNumbers	= 0;
	theMarker		= anInputText;
	while (ReadOneNumber(theMarker, NULL, &theMarker, &theErrorMessage))
	{
		theNumNumbers++;
	}
	if (theErrorMessage != NULL)
		goto CleanUpReadMatrices;

	//	If anInputText contains a set of 4×4 matrices,
	//	the number of numbers should be a positive multiple of 16.
	if (theNumNumbers % 16 != 0)
	{
		theErrorMessage = u"A matrix generator file should contain a list of 4×4 matrices and nothing else.\nUnfortunately the number of entries in the present file is not a multiple of 16.";
		goto CleanUpReadMatrices;
	}

	//	How many generators?
	theNumGenerators = theNumNumbers / 16;

	//	Allocate space for the matrices.
	*aMatrixList = AllocateMatrixList(theNumGenerators);
	if (*aMatrixList == NULL)
	{
		theErrorMessage = u"Couldn't allocate memory for matrix generators.";
		goto CleanUpReadMatrices;
	}

	//	Reread the string, writing the numbers directly into the matrices,
	//	and then compute the determinant to determine the parity.
	theMarker = anInputText;
	for (i = 0; i < theNumGenerators; i++)
	{
		for (j = 0; j < 4; j++)
			for (k = 0; k < 4; k++)
				if ( ! ReadOneNumber(theMarker, &(*aMatrixList)->itsMatrices[i].m[j][k], &theMarker, NULL) )
				{
					theErrorMessage = u"\"Impossible\" error in ReadMatrices().";
					goto CleanUpReadMatrices;
				}
		(*aMatrixList)->itsMatrices[i].itsParity =
			(MatrixDeterminant(&(*aMatrixList)->itsMatrices[i]) > 0.0) ?
			ImagePositive : ImageNegative;
	}

CleanUpReadMatrices:

	if (theErrorMessage != NULL)
		FreeMatrixList(aMatrixList);

	return theErrorMessage;
}


static bool ReadOneNumber(
	Byte		*aString,			//	input, null-terminated string
	double		*aValue,			//	output, may be null
	Byte		**aStoppingPoint,	//	output, may be null, *aStoppingPoint may equal aString
	ErrorText	*anError)			//	output, may be null
{
	char		*theStartingPoint	= NULL,
				*theStoppingPoint	= NULL;
	double		theNumber			= 0.0;

	theStartingPoint = (char *) aString;

	//	The strtod() documentation defines whitespace as spaces and tabs only.
	//	In practice strtod() also skips over newlines, but one hates
	//	to rely on undocumented behavior, so skip over all whitespace
	//	before calling strtod().
	while (*theStartingPoint == ' '
		|| *theStartingPoint == '\t'
		|| *theStartingPoint == '\r'
		|| *theStartingPoint == '\n')
	{
		theStartingPoint++;
	}

	//	Try to read a number.
	theNumber = strtod(theStartingPoint, &theStoppingPoint);

	//	Did we get one?
	//
	//	Note:  When strtod() fails it returns 0.0, making it awkward to distinguish
	//	failure from a successfully read 0.0.   The documentation doesn't say exactly
	//	what to expect from errno when no conversion can be done.
	//	The practical (and safe) approach seems to be to check whether
	//	theStoppingPoint differs from the theStartingPoint.

	if (theStoppingPoint != theStartingPoint)	//	success
	{
		if (aValue != NULL)
			*aValue = theNumber;

		if (aStoppingPoint != NULL)
			*aStoppingPoint = (Byte *) theStoppingPoint;

		if (anError != NULL)
			*anError = NULL;

		return true;
	}
	else										//	failure
	{
		if (aValue != NULL)
			*aValue = 0.0;

		if (aStoppingPoint != NULL)
			*aStoppingPoint = aString;

		//	The only valid reason not to get a number is reaching the end of the string.
		if (anError != NULL)
			*anError = (*theStoppingPoint != 0) ?
				u"Matrix file contains text other than numbers." :	//	bad characters
				NULL;												//	no error

		return false;
	}
}


static ErrorText LoadGenerators(
	ModelData			*md,
	MatrixList			*aGeneratorList,
	HyperbolicSpaceType	aHyperbolicSpaceType)
{
	ErrorText	theErrorMessage					= NULL;
	double		theDirichletDomainOutradius;
	MatrixList	*theProvisionalHolonomyGroup	= NULL,
				*theFullHolonomyGroup			= NULL;
#if (defined(HIGH_RESOLUTION_SCREENSHOT) || defined(SHAPE_OF_SPACE_CH_15) || defined(SHAPE_OF_SPACE_CH_16))
	Matrix		theRotation,
				theTranslation;
#endif

	//	Delete any pre-existing Dirichlet domain and honeycomb,
	//	reset the user's placement and speed, and reset the centerpiece.
	md->itsSpaceType = SpaceNone;
	FreeDirichletDomain(&md->itsDirichletDomain);
	FreeHoneycomb(&md->itsHoneycomb);
	MatrixIdentity(&md->itsUserBodyPlacement);
#ifdef START_OUTSIDE
	md->itsUserSpeed = 2.0 * USER_SPEED_INCREMENT;
#elif defined(PREPARE_FOR_SCREENSHOT)
	md->itsUserSpeed = 0.0;
#else
	md->itsUserSpeed = 8.0 * USER_SPEED_INCREMENT;	//	slow forward motion (but not too slow!)
#endif
#ifdef CENTERPIECE_DISPLACEMENT
	MatrixIdentity(&md->itsCenterpiecePlacement);
#endif

	//	Detect the new geometry and make sure it's consistent.
	theErrorMessage = DetectSpaceType(aGeneratorList, &md->itsSpaceType);
	if (theErrorMessage != NULL)
		goto CleanUpLoadGenerators;
	
	//	Set itsHorizonRadius according to the SpaceType.
	//
	//	A more sophisticated approach would take into account
	//	the translation distances of the generators (assuming
	//	the generators have been efficiently chosen) to tile 
	//	more/less deeply when the fundamental domain is likely 
	//	to be large/small, but the present code doesn't do that.
	switch (md->itsSpaceType)
	{
		case SpaceSpherical:
			//	Any value greater than π will suffice to tile all of S³.
			md->itsHorizonRadius = 3.15;
			break;

		case SpaceFlat:

			//	August 2018
			//
			//	On my 2016 MacBook Pro, now running Curved Spaces on Metal,
			//	Curved Spaces just barely manages 60 fps
			//	in its default square window, but then deteriorates
			//	to a jerky 30-40 fps when drawing full screen.
			//	I can understand why the GPU usage would go up
			//	(more pixels to draw) but I'm mystified about
			//	why the CPU usage would increase so dramatically.
			//	It's not simply a matter of the non-square aspect ratio
			//	requiring more group elements, because a small non-square
			//	window produces no such effect.  My only guess
			//	is that this might be due to thermal throttling:
			//	when the integrated GPU needs to process all those extra pixels,
			//	and thus threatens to heat up the whole CPU-GPU chip,
			//	macOS preemptively throttles back the clock speed,
			//	which means the CPU takes longer to do the same amount of work.
			//	I'm not at all convinced that that explanation is correct,
			//	but it's the only reason I can think of for why
			//	going full screen would increase the CPU load.
			//
			//	My iPad Air 2 and iPod Touch (6th generation)
			//	both comfortably hold 60 fps
			//	when Curved Spaces tiles to radius 11.
			//	Neither is CPU limited.
			//	Not sure why the Mac has that problem in fullscreen mode.
		
			//	The number of tiles grows cubicly with the radius,
			//	so we can afford to tile deeper in the flat case
			//	than in the hyperbolic case.
			md->itsHorizonRadius = 11.0;

			break;

		case SpaceHyperbolic:
			//	The number of tiles grows exponentially with the radius,
			//	so we can't tile too deep in the hyperbolic case.

#if defined(HIGH_RESOLUTION_SCREENSHOT)

			UNUSED_PARAMETER(aHyperbolicSpaceType);

			//	For a static screenshot, speed isn't an issue,
			//	and neither is popping.
			//
			//		Note:  Radius 5.5 is OK, but radius 6.5 generates the Metal error
			//		"instance_id type is not big enough to draw this many instances (122812)".
			//		To eliminate this error, we'd need to replace
			//
			//				ushort	iid	[[ instance_id	]]
			//		with
			//				uint	iid	[[ instance_id	]]
			//
			//		in the GPU vertex function.
			//
			md->itsHorizonRadius = 5.5;

#elif defined(MAKE_SCREENSHOTS)

			//	See comment above about iid needing
			//	to become a uint instead of a ushort
			//	if we go to radius 6.5.
			md->itsHorizonRadius = 5.5;

#elif defined(SHAPE_OF_SPACE_CH_15)

			//	See comment below about iid needing
			//	to become a uint instead of a ushort.
			md->itsHorizonRadius = 6.5;

#elif defined(SHAPE_OF_SPACE_CH_16)

			//	The Seifert-Weber space is big,
			//	so we can afford to tile deep.
			//
			//		Note:  Values from around 6.0 on up require replacing
			//
			//				ushort	iid	[[ instance_id	]]
			//		with
			//				uint	iid	[[ instance_id	]]
			//
			//		in the GPU vertex function.
			//
			md->itsHorizonRadius = 7.0;

#else	//	normal resolution

			if (aHyperbolicSpaceType != HyperbolicSpaceGeneric)
			{
				//	Tile deeper for larger spaces like the mirrored dodecahedron
				//	or the Seifert-Weber space.  Setting
				//
				//		md->itsHorizonRadius = 6.0;  or 5.5 -- see Note immediately above
				//
				//	looks best, but it's still a little slow
				//	on integrated graphics from 2008
				//	(and even in 2018, Curved Spaces is CPU-limited
				//	and triggers thermal throttling).
				//	Maybe in a few more years I can use that radius.
				//	For now be satisfied with a less impressive radius,
				//	to keep a smooth 60 fps even on iOS devices.
				md->itsHorizonRadius = 4.0;
			}
			else
			{
				//	Tile less deep for other hyperbolic spaces,
				//	typically the lowest-volume ones.
				md->itsHorizonRadius = 3.0;
			}

#endif	//	HIGH_RESOLUTION_SCREENSHOT or not

			break;
		
		default:
			md->itsHorizonRadius = 0.0;
			break;
	}

	if (md->itsSpaceType != SpaceHyperbolic)
	{
		//	We face a chicken-and-egg problem:
		//	We need a holonomy group in order to construct a Dirichlet domain,
		//	but we need to know the Dirichlet domain's radius
		//	to ensure that the holonomy group tiles out
		//	to the required radius but no further.
		//	The solution is to create a provisional holonomy group,
		//	use it to construct the Dirichlet domain,
		//	and then replace the provisional holonomy group
		//	with a slight larger permanent one.

		//	Use the generators to construct the provisional holonomy group
		//	out to the desired tiling radius, but with no allowance
		//	for the radius of the Dirichlet domain.
		//	Assume the group is discrete and no element fixes the origin.
		theErrorMessage = ConstructHolonomyGroup(
							aGeneratorList,
							md->itsHorizonRadius,
							&theProvisionalHolonomyGroup);
		if (theErrorMessage != NULL)
			goto CleanUpLoadGenerators;

		//	Use the provisional holonomy group to construct a Dirichlet domain.
		theErrorMessage = ConstructDirichletDomain(
							theProvisionalHolonomyGroup,
							&md->itsDirichletDomain);
		if (theErrorMessage != NULL)
			goto CleanUpLoadGenerators;
		
		//	Free theProvisionalHolonomyGroup before constructing theFullHolonomyGroup.
		FreeMatrixList(&theProvisionalHolonomyGroup);

		//	Use the generators and the Dirichlet domain radius
		//	to construct the full holonomy group, allowing for the fact
		//	that a translate of the Dirichlet domain might overlap
		//	the tiling sphere even if that translate's center
		//	lies outside the tiling sphere.  More precisely,
		//	because the user may fly up to theDirichletDomainOutradius units
		//	away from the origin (before a generating matrix moves him/her
		//	to an equivalent but closer position), and the Dirichlet domain's
		//	content may sit up to theDirichletDomainOutradius units away
		//	from the Dirichlet domain's center, we want to include all translates
		//	of the Dirichlet domain whose center sits within
		//
		//		aHorizonRadius  +  2 * theDirichletDomainOutradius
		//
		//	units of the origin.
		//
		//	Assume the group is discrete and no element fixes the origin.
		//
		theDirichletDomainOutradius = DirichletDomainOutradius(md->itsDirichletDomain);
		theErrorMessage = ConstructHolonomyGroup(
							aGeneratorList,
							md->itsHorizonRadius  +  2.0 * theDirichletDomainOutradius,
							&theFullHolonomyGroup);
		if (theErrorMessage != NULL)
			goto CleanUpLoadGenerators;
	}
	else	//	md->itsSpaceType == SpaceHyperbolic
	{
		//	The number of images in a hyperbolic tiling
		//	grows exponentially fast as a function of the tiling radius.
		//	For a space with a large fundamental domain
		//	-- for example the mirrored dodecahedron, which
		//	has outradius 1.22… -- asking ConstructHolonomyGroup()
		//	to tile out an extra 2.44… units would make
		//	the computation unacceptably slow,
		//	and on my iPod Touch it even causes iOS
		//	to terminate the app for using too much memory.
		//
		//	To avoid those problems, for hyperbolic spaces we'll tile
		//	only as far as itsHorizonRadius with a small amount of padding,
		//	ignoring theDirichletDomainOutradius.  This approach
		//	introduces some "popping" as images come into view,
		//	but in practice the popping is hardly noticeable
		//	because the scenery elements already very thin
		//	at this distance in hyperbolic space.
		//

		//	Use the generators to construct the provisional holonomy group
		//	out to the desired tiling radius, but with no allowance
		//	for the radius of the Dirichlet domain.
		//	Assume the group is discrete and no element fixes the origin.
		theErrorMessage = ConstructHolonomyGroup(
							aGeneratorList,
							md->itsHorizonRadius + HYPERBOLIC_TILING_RADIUS_PADDING,
							&theFullHolonomyGroup);
		if (theErrorMessage != NULL)
			goto CleanUpLoadGenerators;

		//	Use the provisional holonomy group to construct a Dirichlet domain.
		theErrorMessage = ConstructDirichletDomain(
							theFullHolonomyGroup,
							&md->itsDirichletDomain);
		if (theErrorMessage != NULL)
			goto CleanUpLoadGenerators;
	}

	//	In the case of a spherical space, we'll want to draw the back hemisphere
	//	if and only if the holonomy group does not contain the antipodal matrix.
	theErrorMessage = NeedsBackHemisphere(theFullHolonomyGroup, md->itsSpaceType, &md->itsDrawBackHemisphere);
	if (theErrorMessage != NULL)
		goto CleanUpLoadGenerators;
	
	//	The space is a 3-sphere iff theHolonomyGroup 
	//	contains the identity matrix alone.
	md->itsThreeSphereFlag = (theFullHolonomyGroup->itsNumMatrices == 1);

	//	Use the holonomy group and the Dirichlet domain
	//	to construct a honeycomb.
	theErrorMessage = ConstructHoneycomb(	theFullHolonomyGroup,
											md->itsDirichletDomain,
											&md->itsHoneycomb);
	if (theErrorMessage != NULL)
		goto CleanUpLoadGenerators;
	
	//	Free theFullHolonomyGroup.
	FreeMatrixList(&theFullHolonomyGroup);

	//	The Dirichlet domain has changed, so let the platform-dependent code
	//	know that it needs to re-create the meshes that it uses
	//	to represent the walls and the vertex figures (if present).
	md->itsDirichletWallsMeshNeedsRefresh	= true;
	md->itsVertexFigureMeshNeedsReplacement	= true;

#ifdef CENTERPIECE_DISPLACEMENT
	//	For ad hoc convenience in the Shape of Space lecture,
	//	move the user back a bit, move the centerpiece forward a bit,
	//	and set the speed to zero.
	//	This will look good when the fundamental domain is a unit cube.
	//
	//	Technical note:  When the aperture is closed and 
	//	only the central Dirichlet domain is drawn, it's crucial that 
	//	we place the user at -1/2 + ε rather that at -1/2, so the user
	//	doesn't land at +1/2 instead.  Also, we want to have at least
	//	a near clipping distance's margin between the user and the back wall,
	//	in case s/he turns around!
	//
	MatrixTranslation(&md->itsUserBodyPlacement, md->itsSpaceType, 0.0, 0.0, -0.49);
	MatrixTranslation(&md->itsCenterpiecePlacement, md->itsSpaceType, 0.0, 0.0, 0.25);
	md->itsUserSpeed = 0.0;
#endif
#ifdef START_STILL
	//	For ad hoc convenience in the Shape of Space lecture,
	//	move the user back a bit and set the speed to zero.
	MatrixTranslation(&md->itsUserBodyPlacement, md->itsSpaceType, 0.0, 0.0, -0.49);
	md->itsUserSpeed = 0.0;
#endif
#if (defined(HIGH_RESOLUTION_SCREENSHOT) || (SHAPE_OF_SPACE_CH_16 == 3))
	//	Ad hoc placement for viewing dodecahedron
	MatrixRotation(&theRotation, 0.0, SafeAcos(cos(PI/3)/sin(PI/5)), 0.0);
#if (defined(HIGH_RESOLUTION_SCREENSHOT))
	MatrixTranslation(&theTranslation, md->itsSpaceType, 0.0, 0.0, -0.125);
#else
	MatrixIdentity(&theTranslation);
#endif
	//	Ultimately theViewMatrix will be the inverse of itsUserBodyPlacement,
	//	so we must multiply the factors here in a possibly unexpected order.
	MatrixProduct(&theTranslation, &theRotation, &md->itsUserBodyPlacement);
	md->itsUserSpeed = 0.0;
#endif	//	HIGH_RESOLUTION_SCREENSHOT || SHAPE_OF_SPACE_CH_16
#ifdef SHAPE_OF_SPACE_CH_15
	Matrix	theInitialPlacementInMirroredDodecahedron =
	{
		{
			{ 0.85065080835203999,  0.00000000000000000, -0.52573111211913359,  0.00000000000000055},
			{ 0.00000000000000000,  1.00000000000000000,  0.00000000000000000,  0.00000000000000000},
			{ 0.70261593828905788,  0.00000000000000000,  1.13685646918909544, -0.88662945376008673},
			{-0.46612868876286961,  0.00000000000000000, -0.75421206154974574,  1.33645493312528485}
		},
		ImagePositive
	};

	md->itsUserBodyPlacement	= theInitialPlacementInMirroredDodecahedron;
	md->itsUserSpeed			= 0.0;
#endif
#if (SHAPE_OF_SPACE_CH_16 == 6)
	Matrix	theInitialPlacementInPDS =
	{
		{
			{ 0.80640807679039528, -0.30789150884337557, -0.50487771385008196,  0.00270675578517899},
			{ 0.24471800986291653,  0.95095974716323428, -0.18901291012249430,  0.00792305061178555},
			{ 0.53714479936460013,  0.02928877281824072,  0.83968564844840987, -0.07446908145088317},
			{ 0.03598018692993523, -0.00453275238554037,  0.06557915563892300,  0.99718817414266625}
		},
		ImagePositive
	};

	md->itsUserBodyPlacement	= theInitialPlacementInPDS;
	md->itsUserSpeed			= 0.0;
#endif
#ifdef SHAPE_OF_SPACE_CH_7
	//	Set z ~ -1.0 to ensure that the cube at the lower left is in "home position",
	//	with a red right face, a yellow top face and a a blue near face.
	MatrixTranslation(&md->itsUserBodyPlacement, md->itsSpaceType, 0.5, 0.5, -0.804);
	md->itsUserSpeed = 0.0;
#endif

CleanUpLoadGenerators:

	if (theErrorMessage != NULL)
	{
		FreeDirichletDomain(&md->itsDirichletDomain);
		FreeHoneycomb(&md->itsHoneycomb);
	}

	md->itsChangeCount++;

	FreeMatrixList(&theProvisionalHolonomyGroup);
	FreeMatrixList(&theFullHolonomyGroup);

	return theErrorMessage;
}


static ErrorText DetectSpaceType(
	MatrixList		*aGeneratorList,
	SpaceType		*aSpaceType)
{
	SpaceType		theSpaceType	= SpaceNone;
	unsigned int	i;
	
	//	Special case:
	//	If no generators are present, the space is a 3-sphere.
	if (aGeneratorList->itsNumMatrices == 0)
	{
		*aSpaceType = SpaceSpherical;
		return NULL;
	}

	//	Generic case:
	//	Set *aSpaceType to the type of the first generator,
	//	then make sure all the rest agree.

	*aSpaceType = SpaceNone;

	for (i = 0; i < aGeneratorList->itsNumMatrices; i++)
	{
		if (aGeneratorList->itsMatrices[i].m[3][3] <  1.0)
			theSpaceType = SpaceSpherical;
		if (aGeneratorList->itsMatrices[i].m[3][3] == 1.0)
			theSpaceType = SpaceFlat;
		if (aGeneratorList->itsMatrices[i].m[3][3] >  1.0)
			theSpaceType = SpaceHyperbolic;

		if (*aSpaceType == SpaceNone)
			*aSpaceType = theSpaceType;
		else
		{
			if (*aSpaceType != theSpaceType)
			{
				*aSpaceType = SpaceNone;
				return u"Matrix generators have inconsistent geometries (spherical, flat, hyperbolic), or perhaps an unneeded identity matrix is present.";
			}
		}
	}

	return NULL;
}
