//////////
//
//	File:		QTCompress.c
//
//	Contains:	Sample code for using QuickTime's standard image compression dialog component.
//
//	Written by:	Tim Monroe
//				Based on existing code by Apple Developer Technical Support, which was itself
//				based on the code in Chapter 3 of Inside Macintosh: QuickTime Components.
//
//	Copyright:	� 1998 by Apple Computer, Inc., all rights reserved.
//
//	Change History (most recent first):
//
//	   <2>	 	11/11/00	rtm		added ability to compress an image sequence (based largely on
//									code from ConvertToMovieJr.c)
//	   <1>	 	11/01/00	rtm		first file from QTStdCompr.c (in QTGoodies)
//	   
//	This sample code illustrates how to use QuickTime's standard image compression dialog component
//	to get compression settings from the user and to compress an image or image sequence using those settings. See
//	Chapter 3 of Inside Macintosh: QuickTime Components for complete information on the standard
//	image compression dialog routines.
//
//	This sample also shows how to extend the basic user interface by installing a modal-dialog filter
//	function and a hook function to handle the optional custom button in the dialog box. If you don't
//	want this extended behavior, set gUseExtendedProcs to false.
//
//	NOTES:
//
//	*** (1) ***
//	Using the SCCompressImage function to compress a pixmap using some of the available compression
//	types (for instance, BMP) results in a block of compressed data that does not contain the required
//	headers. As a result, saving that data into a file results in an invalid image file. This is a
//	known limitation of QuickTime 3 and may be fixed in the future. Currently the only way to generate
//	these headers is to use a graphics importer to export the file as a BMP (or whatever) file. This
//	is NOT illustrated in this sample code.
//	
//	*** (2) ***
//	You can use the SCSetInfo function with the scSettingsStateType selector to retrieve a handle
//	containing the current compression settings; this might be useful if you were allowing the user
//	to compress a series of images and wanted to preserve the user's settings from one image to the
//	next (instead of reverting to the defaults for every image). Note, however, that the data in
//	that handle is byte-ordered according to the platform the code is running on. As a result, you
//	should not store that data in a file and expect that file to be valid on other platforms. To
//	get a handle of data in a platform-independent format, use the function SCGetSettingsAsAtomContainer
//	(introduced in QuickTime 3); to restore the settings in that handle, use the related function
//	SCSetSettingsAsAtomContainer.
//	
//////////

//////////
//
// header files
//	   
//////////

#include "QTCompress.h"


//////////
//
// global variables
//	   
//////////

Boolean							gUseExtendedProcs = true;	// do we use extended procs with our dialog box?
SCExtendedProcs 				gProcStruct;


#if TARGET_OS_MAC
extern void QTFrame_HandleEvent (EventRecord *theEvent);	// our Mac application's event-handling function
#endif


//////////
//
// QTCmpr_CompressImage
// Compress an image.
//
//////////

void QTCmpr_CompressImage (WindowObject theWindowObject)
{
	Rect						myRect;
	GraphicsImportComponent		myImporter = NULL;
	ComponentInstance			myComponent = NULL;
	GWorldPtr					myImageWorld = NULL;		// the graphics world we draw the image in
	PixMapHandle				myPixMap = NULL;
	ImageDescriptionHandle		myDesc = NULL;
	Handle						myHandle = NULL;
	OSErr						myErr = noErr;

	if (theWindowObject == NULL)
		return;
		
	//////////
	//
	// get a graphics importer for the image file and determine the natural size of the image;
	// note that the image file *already* has a graphics importer associated with it (namely
	// (**theWindowObject).fGraphicsImporter), but we create a new one so that the existing one
	// can be used to redraw the image in the callback procedure QTCmpr_FilterProc
	//
	//////////

	myErr = GetGraphicsImporterForFile(&(**theWindowObject).fFileFSSpec, &myImporter);
	if (myErr != noErr)
		goto bail;
	
	myErr = GraphicsImportGetNaturalBounds(myImporter, &myRect);
	if (myErr != noErr)
		goto bail;

	//////////
	//
	// create an offscreen graphics world and draw the image into it
	//
	//////////
	
	myErr = QTNewGWorld(&myImageWorld, 0, &myRect, NULL, NULL, kICMTempThenAppMemory);
	if (myErr != noErr)
		goto bail;
	
	// get the pixmap of the GWorld; we'll lock the pixmap, just to be safe
	myPixMap = GetGWorldPixMap(myImageWorld);
	if (!LockPixels(myPixMap))
		goto bail;
	
	// set the current port and draw the image
	GraphicsImportSetGWorld(myImporter, (CGrafPtr)myImageWorld, NULL);
	GraphicsImportDraw(myImporter);
	
	//////////
	//
	// configure and display the standard image compression dialog box
	//
	//////////
	
	// open the standard compression dialog component
	myComponent = OpenDefaultComponent(StandardCompressionType, StandardCompressionSubType);
	if (myComponent == NULL)
		goto bail;
		
	// set the picture to be displayed in the dialog box; passing NULL for the rect
	// means use the entire image; passing 0 for the flags means to use the default
	// system method of displaying the test image, which is currently a combination
	// of cropping and scaling; personally, I prefer scaling (your mileage may vary)
	SCSetTestImagePixMap(myComponent, myPixMap, NULL, scPreferScaling);

	// install the custom procs, if requested
	// we can install two kinds of custom procedures for use in connection with
	// the standard dialog box: (1) a modal-dialog filter function, and (2) a hook
	// function to handle the custom button in the dialog box
	if (gUseExtendedProcs)
		QTCmpr_InstallExtendedProcs(myComponent, (long)myPixMap);
	
	// request image compression settings from the user; in other words, put up the dialog box
	myErr = SCRequestImageSettings(myComponent);
	if (myErr == scUserCancelled)
		goto bail;

	//////////
	//
	// compress the image
	//
	//////////
	
	myErr = SCCompressImage(myComponent, myPixMap, NULL, &myDesc, &myHandle);
	if (myErr != noErr)
		goto bail;

	//////////
	//
	// save the compressed image in a new file
	//
	//////////
	
	QTCmpr_PromptUserForDiskFileAndSaveCompressed(myHandle, myDesc);
	
bail:
	if (gUseExtendedProcs)
		QTCmpr_RemoveExtendedProcs();

	if (myPixMap != NULL)
		if (GetPixelsState(myPixMap) & pixelsLocked)
			UnlockPixels(myPixMap);
	
	if (myImporter != NULL)
		CloseComponent(myImporter);

	if (myComponent != NULL)
		CloseComponent(myComponent);

	if (myDesc != NULL)
		DisposeHandle((Handle)myDesc);

	if (myHandle != NULL)
		DisposeHandle(myHandle);

	if (myImageWorld != NULL)
		DisposeGWorld(myImageWorld);
}


//////////
//
// QTCmpr_PromptUserForDiskFileAndSaveCompressed
// Let the user select a disk file, then write the compressed image into that file.
//
//////////

void QTCmpr_PromptUserForDiskFileAndSaveCompressed (Handle theHandle, ImageDescriptionHandle theDesc)
{
	FSSpec				myFile;
	Boolean				myIsSelected = false;
	Boolean				myIsReplacing = false;	
	short				myRefNum = -1;
	StringPtr 			myImagePrompt = QTUtils_ConvertCToPascalString(kQTCSaveImagePrompt);
	StringPtr 			myImageFileName = QTUtils_ConvertCToPascalString(kQTCSaveImageFileName);
	OSErr				myErr = noErr;

	// do a little parameter checking....
	if ((theHandle == NULL) || (theDesc == NULL))
		goto bail;
		
	if ((**theDesc).dataSize > GetHandleSize(theHandle))
		goto bail;

	// prompt the user for a file to put the compressed image into; in theory, the name
	// should have a file extension appropriate to the type of compressed data selected by the user;
	// this is left as an exercise for the reader
	QTFrame_PutFile(myImagePrompt, myImageFileName, &myFile, &myIsSelected, &myIsReplacing);
	if (!myIsSelected)
		goto bail;

	HLock(theHandle);

	// create and open the file
	myErr = FSpCreate(&myFile, kImageFileCreator, (**theDesc).cType, 0);
	
	if (myErr == noErr)
		myErr = FSpOpenDF(&myFile, fsRdWrPerm, &myRefNum);
		
	if (myErr == noErr)
		myErr = SetFPos(myRefNum, fsFromStart, 0);

	// now write the data in theHandle into the file
	if (myErr == noErr)
		myErr = FSWrite(myRefNum, &(**theDesc).dataSize, *theHandle);
	
	if (myErr == noErr)
		myErr = SetFPos(myRefNum, fsFromStart, (**theDesc).dataSize);

	if (myErr == noErr)
		myErr = SetEOF(myRefNum, (**theDesc).dataSize);
				 
	if (myRefNum != -1)
		myErr = FSClose(myRefNum);
		
bail:
	free(myImagePrompt);
	free(myImageFileName);

	HUnlock(theHandle);
}


//////////
//
// QTCmpr_CompressSequence
// Compress an image sequence (that is, all the frames of a movie).
//
// Based on existing ConvertToMovieJr.c source code.
//
//////////

void QTCmpr_CompressSequence (WindowObject theWindowObject)
{
	ComponentInstance			myComponent = NULL;
	GWorldPtr					myImageWorld = NULL;		// the graphics world we draw the images in
	PixMapHandle				myPixMap = NULL;
	Movie						mySrcMovie = NULL;
	Track						mySrcTrack = NULL;
	Movie						myDstMovie = NULL;
	Track						myDstTrack = NULL;
	Media						myDstMedia = NULL;
	Rect						myRect;
	PicHandle					myPicture = NULL;
	CGrafPtr					mySavedPort = NULL;
	GDHandle					mySavedDevice = NULL;
	SCTemporalSettings			myTimeSettings;
	SCDataRateSettings			myRateSettings;
	FSSpec						myFile;
	Boolean						myIsSelected = false;
	Boolean						myIsReplacing = false;	
	short						myRefNum = -1;
	StringPtr 					myMoviePrompt = QTUtils_ConvertCToPascalString(kQTCSaveMoviePrompt);
	StringPtr 					myMovieFileName = QTUtils_ConvertCToPascalString(kQTCSaveMovieFileName);
	MatrixRecord				myMatrix;
	ImageDescriptionHandle		myImageDesc = NULL;
	TimeValue					myCurMovieTime = 0L;
	TimeValue					myOrigMovieTime = 0L;		// current movie time, when compression is begun
	short						myFrameNum;		
	long						myFlags = 0L;
	long						myNumFrames = 0L;
	long						mySrcMovieDuration = 0L;	// duration of source movie
	OSErr						myErr = noErr;
#if USE_ASYNC_COMPRESSION
	ICMCompletionProcRecord		myICMComplProcRec;
	ICMCompletionProcRecordPtr	myICMComplProcPtr = NULL;
	OSErr						myICMComplProcErr = noErr;

	myICMComplProcRec.completionProc = NULL;
	myICMComplProcRec.completionRefCon = 0L;
#endif

	if (theWindowObject == NULL)
		goto bail;

	//////////
	//
	// get the movie and the first video track in the movie
	//
	//////////
	
	mySrcMovie = (**theWindowObject).fMovie;
	if (mySrcMovie == NULL)
		goto bail;

	mySrcTrack = GetMovieIndTrackType(mySrcMovie, 1, VideoMediaType, movieTrackMediaType);
	if (mySrcTrack == NULL)
		goto bail;
	
	// stop the movie; we don't want it to be playing while we're (re)compressing it
	SetMovieRate(mySrcMovie, (Fixed)0L);

	// get the current movie time, when compression is begun; we'll restore this later
	myOrigMovieTime = GetMovieTime(mySrcMovie, NULL);

	//////////
	//
	// configure and display the Standard Image Compression dialog box
	//
	//////////
	
	// open an instance of the Standard Image Compression dialog component
	myComponent = OpenDefaultComponent(StandardCompressionType, StandardCompressionSubType);
	if (myComponent == NULL)
		goto bail;

	// turn off "best depth" option in the compression dialog, because all of our
	// buffering is done at 32-bits (regardless of the depth of the source data)
	//
	// a more ambitious approach would be to loop through each of the video sample
	// descriptions in each of the video tracks looking for the deepest depth, and
	// using that for the best depth; better yet, we could find out which compressors
	// were used and set one of those as the default in the compression dialog
	SCGetInfo(myComponent, scPreferenceFlagsType, &myFlags);
	myFlags &= ~scShowBestDepth;
	SCSetInfo(myComponent, scPreferenceFlagsType, &myFlags);

	// because we are recompressing a movie that may have a variable frame rate,
	// we want to allow the user to leave the frame rate text field blank (in which
	// case we can preserve the frame durations of the source movie); if the user
	// enters a number, we will resample the movie at a new frame rate; if we don't
	// clear this flag, the compression dialog will not allow zero in the frame rate field
	//
	// NOTE: we could have set this flag above when we cleared the scShowBestDepth flag;
	// it is done here for clarity.	
	SCGetInfo(myComponent, scPreferenceFlagsType, &myFlags);
	myFlags |= scAllowZeroFrameRate;
	SCSetInfo(myComponent, scPreferenceFlagsType, &myFlags);

	// get the number of video frames in the movie
	myNumFrames = QTUtils_GetFrameCount(mySrcTrack);

	// get the bounding rectangle of the movie, create a 32-bit GWorld with those
	// dimensions, and draw the movie poster picture into it; this GWorld will be
	// used for the test image in the compression dialog box and for rendering movie
	// frames
	myPicture = GetMoviePosterPict(mySrcMovie);
	if (myPicture == NULL)
		goto bail;
		
	GetMovieBox(mySrcMovie, &myRect);

	myErr = NewGWorld(&myImageWorld, 32, &myRect, NULL, NULL, 0L);
	if (myErr != noErr)
		goto bail;
		
	// get the pixmap of the GWorld; we'll lock the pixmap, just to be safe
	myPixMap = GetGWorldPixMap(myImageWorld);
	if (!LockPixels(myPixMap))
		goto bail;

	// draw the movie poster image into the GWorld
	GetGWorld(&mySavedPort, &mySavedDevice);
	SetGWorld(myImageWorld, NULL);
	EraseRect(&myRect);
	DrawPicture(myPicture, &myRect);
	KillPicture(myPicture);
	SetGWorld(mySavedPort, mySavedDevice);

	// set the picture to be displayed in the dialog box; passing NULL for the rect
	// means use the entire image; passing 0 for the flags means to use the default
	// system method of displaying the test image, which is currently a combination
	// of cropping and scaling; personally, I prefer scaling (your mileage may vary)
	SCSetTestImagePixMap(myComponent, myPixMap, NULL, scPreferScaling);

	// install the custom procs, if requested
	// we can install two kinds of custom procedures for use in connection with
	// the standard dialog box: (1) a modal-dialog filter function, and (2) a hook
	// function to handle the custom button in the dialog box
	if (gUseExtendedProcs)
		QTCmpr_InstallExtendedProcs(myComponent, (long)myPixMap);
	
	// set up some default settings for the compression dialog
	SCDefaultPixMapSettings(myComponent, myPixMap, true);
	
	// clear out the default frame rate chosen by Standard Compression (a frame rate
	// of 0 means to use the rate of the source movie)
	myErr = SCGetInfo(myComponent, scTemporalSettingsType, &myTimeSettings);
	if (myErr != noErr)
		goto bail;

	myTimeSettings.frameRate = 0;
	SCSetInfo(myComponent, scTemporalSettingsType, &myTimeSettings);

	// request image compression settings from the user; in other words, put up the dialog box
	myErr = SCRequestSequenceSettings(myComponent);
	if (myErr == scUserCancelled)
		goto bail;

	// get a copy of the temporal settings the user entered; we'll need them for some
	// of our calculations (in a simpler application, we'd never have to look at them)	
	SCGetInfo(myComponent, scTemporalSettingsType, &myTimeSettings);

	//////////
	//
	// adjust the data rate [to be supplied][relevant only for movies that have sound tracks]
	//
	//////////

	
	//////////
	//
	// adjust the sample count
	//
	// if the user wants to resample the frame rate of the movie (as indicated a non-zero
	// value in the frame rate field) calculate the number of frames and duration for the new movie
	//
	//////////
	
	if (myTimeSettings.frameRate != 0) {
		long	myDuration = GetMovieDuration(mySrcMovie);
		long	myTimeScale = GetMovieTimeScale(mySrcMovie);
		float	myFloat = (float)myDuration * myTimeSettings.frameRate;
		
		myNumFrames = myFloat / myTimeScale / 65536;
		if (myNumFrames == 0)
			myNumFrames = 1;
	}

	//////////
	//
	// get the name and location of the new movie file
	//
	//////////

	// prompt the user for a file to put the compressed image into; in theory, the name
	// should have a file extension appropriate to the type of compressed data selected by the user;
	// this is left as an exercise for the reader
	QTFrame_PutFile(myMoviePrompt, myMovieFileName, &myFile, &myIsSelected, &myIsReplacing);
	if (!myIsSelected)
		goto bail;

	// delete any existing file of that name
	if (myIsReplacing) {
		myErr = DeleteMovieFile(&myFile);
		if (myErr != noErr)
			goto bail;
	}
		
	//////////
	//
	// create the target movie
	//
	//////////
	
	myErr = CreateMovieFile(&myFile, sigMoviePlayer, smSystemScript, 
								createMovieFileDeleteCurFile | createMovieFileDontCreateResFile, &myRefNum, &myDstMovie);
	if (myErr != noErr)
		goto bail;
	
	// create a new video movie track with the same dimensions as the entire source movie
	myDstTrack = NewMovieTrack(myDstMovie,
								(long)(myRect.right - myRect.left) << 16,
								(long)(myRect.bottom - myRect.top) << 16, kNoVolume);
	if (myDstTrack == NULL)
		goto bail;
	
	// create a media for the new track with the same time scale as the source movie;
	// because the time scales are the same, we don't have to do any time scale conversions.
	myDstMedia = NewTrackMedia(myDstTrack, VIDEO_TYPE, GetMovieTimeScale(mySrcMovie), 0, 0);
	if (myDstMedia == NULL)
		goto bail;
	
	// copy the user data and settings from the source to the dest movie
	CopyMovieSettings(mySrcMovie, myDstMovie);
	
	// set movie matrix to identity and clear the movie clip region (because the conversion
	// process transforms and composites all video tracks into one untransformed video track)
	SetIdentityMatrix(&myMatrix);
	SetMovieMatrix(myDstMovie, &myMatrix);
	SetMovieClipRgn(myDstMovie, NULL);
	
	// set the movie to highest quality imaging
	SetMoviePlayHints(mySrcMovie, hintsHighQuality, hintsHighQuality);

	myImageDesc = (ImageDescriptionHandle)NewHandleClear(sizeof(ImageDescription));
	if (myImageDesc == NULL)
		goto bail;

	// prepare for adding frames to the movie
	myErr = BeginMediaEdits(myDstMedia);
	if (myErr != noErr)
		goto bail;

	//////////
	//
	// compress the image sequence
	//
	// we are going to step through the source movie, compress each frame, and then add
	// the compressed frame to the destination movie
	//
	//////////
	
	myErr = SCCompressSequenceBegin(myComponent, myPixMap, NULL, &myImageDesc);
	if (myErr != noErr)
		goto bail;
	
#if USE_ASYNC_COMPRESSION
	myFlags = codecFlagUpdatePrevious + codecFlagUpdatePreviousComp + codecFlagLiveGrab;
	SCSetInfo(myComponent, scCodecFlagsType, &myFlags);
#endif

	// clear out our image GWorld and set movie to draw into it
	SetGWorld(myImageWorld, NULL);
	EraseRect(&myRect);
	SetMovieGWorld(mySrcMovie, myImageWorld, GetGWorldDevice(myImageWorld));

	// set current time value to beginning of the source movie
	myCurMovieTime = 0;

	// get a value we'll need inside the loop
	mySrcMovieDuration = GetMovieDuration(mySrcMovie);

	// loop through all of the interesting times we counted above
	for (myFrameNum = 0; myFrameNum < myNumFrames; myFrameNum++) {
		short			mySyncFlag;
		TimeValue		myDuration;
		long			myDataSize;
		Handle			myCompressedData;

		//////////
		//
		// get the next frame of the source movie
		//
		//////////
		
		// if we are resampling the movie, step to the next frame
		if (myTimeSettings.frameRate) {
			myCurMovieTime = myFrameNum * mySrcMovieDuration / (myNumFrames - 1);
			myDuration = mySrcMovieDuration / myNumFrames;
		} else {
			OSType		myMediaType = VIDEO_TYPE;
			
			myFlags = nextTimeMediaSample;

			// if this is the first frame, include the frame we are currently on		
			if (myFrameNum == 0)
				myFlags |= nextTimeEdgeOK;
			
			// if we are maintaining the frame durations of the source movie,
			// skip to the next interesting time and get the duration for that frame
			GetMovieNextInterestingTime(mySrcMovie, myFlags, 1, &myMediaType, myCurMovieTime, 0, &myCurMovieTime, &myDuration);
		}
		
		SetMovieTimeValue(mySrcMovie, myCurMovieTime);
		MoviesTask(mySrcMovie, 0);
		MoviesTask(mySrcMovie, 0);
		MoviesTask(mySrcMovie, 0);

		// if data rate constraining is being done, tell Standard Compression the
		// duration of the current frame in milliseconds; we only need to do this
		// if the frames have variable durations
		if (!SCGetInfo(myComponent, scDataRateSettingsType, &myRateSettings)) {
			myRateSettings.frameDuration = myDuration * 1000 / GetMovieTimeScale(mySrcMovie);
			SCSetInfo(myComponent, scDataRateSettingsType, &myRateSettings);
		}

		//////////
		//
		// compress the current frame of the source movie and add it to the destination movie
		//
		//////////
		
		// if SCCompressSequenceFrame completes successfully, myCompressedData will hold
		// a handle to the newly-compressed image data and myDataSize will be the size of
		// the compressed data (which will usually be different from the size of the handle);
		// also mySyncFlag will be a value that that indicates whether or not the frame is a
		// key frame (and which we pass directly to AddMediaSample); note that we do not need
		// to dispose of myCompressedData, since SCCompressSequenceEnd will do that for us
#if !USE_ASYNC_COMPRESSION
		myErr = SCCompressSequenceFrame(myComponent, myPixMap, &myRect, &myCompressedData, &myDataSize, &mySyncFlag);
		if (myErr != noErr)
			goto bail;
#else
		if (myICMComplProcPtr == NULL) {
			myICMComplProcRec.completionProc = NewICMCompletionProc(QTCmpr_CompletionProc);
			myICMComplProcRec.completionRefCon = (long)&myICMComplProcErr;
			myICMComplProcPtr = &myICMComplProcRec;
		}
		
		myICMComplProcErr = kAsyncDefaultValue;
		
		myErr = SCCompressSequenceFrameAsync(myComponent, myPixMap, &myRect, &myCompressedData, &myDataSize, &mySyncFlag, myICMComplProcPtr);
		if (myErr != noErr)
			goto bail;

		// spin our wheels while we're waiting for the compress call to complete
		while (myICMComplProcErr == kAsyncDefaultValue) {
			EventRecord			myEvent;
			
			WaitNextEvent(0, &myEvent, 60, NULL);
			SCAsyncIdle(myComponent);
		}
		myErr = myICMComplProcErr;
#endif

		myErr = AddMediaSample(myDstMedia, myCompressedData, 0, myDataSize, myDuration, (SampleDescriptionHandle)myImageDesc, 1, mySyncFlag, NULL);
		if (myErr != noErr)
			goto bail;
	}
	
	// close the compression sequence; this will dispose of the image description
	// and compressed data handles allocated by SCCompressSequenceBegin
	SCCompressSequenceEnd(myComponent);

	//////////
	//
	// add the media data to the destination movie
	//
	//////////
	
	myErr = EndMediaEdits(myDstMedia);
	if (myErr != noErr)
		goto bail;
	
	InsertMediaIntoTrack(myDstTrack, 0, 0, GetMediaDuration(myDstMedia), fixed1);

	// add the movie resource to the dst movie file.
	myErr = AddMovieResource(myDstMovie, myRefNum, NULL, NULL);
	if (myErr != noErr)
		goto bail;

	// flatten the movie data [to be supplied]
	
	// close the movie file
	CloseMovieFile(myRefNum);
	
bail:
	// close the Standard Compression component
	if (myComponent != NULL)
		CloseComponent(myComponent);

	if (mySrcMovie != NULL) {
		// restore the source movie's original graphics port and device
		SetMovieGWorld(mySrcMovie, mySavedPort, mySavedDevice);

		// restore the source movie's original movie time
		SetMovieTimeValue(mySrcMovie, myOrigMovieTime);
	}
	
	// restore the original graphics port and device
	SetGWorld(mySavedPort, mySavedDevice);

	// delete the GWorld we were drawing frames into
	if (myImageWorld != NULL)
		DisposeGWorld(myImageWorld);
	
#if USE_ASYNC_COMPRESSION
	if (myICMComplProcRec.completionProc != NULL)
		DisposeICMCompletionUPP(myICMComplProcRec.completionProc);
#endif

	free(myMoviePrompt);
	free(myMovieFileName);
}


//////////
//
// QTCmpr_InstallExtendedProcs
// Install the modal-dialog filter function and the hook function.
//
//////////

static void QTCmpr_InstallExtendedProcs (ComponentInstance theComponent, long theRefCon)
{
	StringPtr 		myButtonTitle = QTUtils_ConvertCToPascalString("Defaults");

	// the modal-dialog filter function can be used to handle any events that
	// the standard image compression dialog handler doesn't know about, such
	// as any update events for windows owned by the application
	gProcStruct.filterProc = NewSCModalFilterUPP(QTCmpr_FilterProc);

#if USE_CUSTOM_BUTTON	
	// the hook function can be used to handle clicks on the custom button
	gProcStruct.hookProc = NewSCModalHookUPP(QTCmpr_ButtonProc);
	
	// copy the string for our custom button into the extended procs structure
	BlockMove(myButtonTitle, gProcStruct.customName, myButtonTitle[0] + 1);
#else
	gProcStruct.hookProc = NULL;
	gProcStruct.customName[0] = 0;
#endif

	// in this example, we pass the pixel map handle as a refcon
	gProcStruct.refcon = theRefCon;
	
	// set the current extended procs
	SCSetInfo(theComponent, scExtendedProcsType, &gProcStruct);
	
	free(myButtonTitle);
}


//////////
//
// QTCmpr_RemoveExtendedProcs
// Remove the modal-dialog filter function and the hook function.
//
//////////

static void QTCmpr_RemoveExtendedProcs (void)
{
	// clear out the extended procedures
	SCSetInfo((ComponentInstance)gProcStruct.refcon, scExtendedProcsType, NULL);
	
	// dispose of the routine descriptors
	if (gProcStruct.filterProc != NULL)
		DisposeSCModalFilterUPP(gProcStruct.filterProc);
		
	if (gProcStruct.hookProc != NULL)
		DisposeSCModalHookUPP(gProcStruct.hookProc);
	
	// clear out our global extended procs structure
	gProcStruct.filterProc = NULL;
	gProcStruct.hookProc = NULL;
	gProcStruct.customName[0] = 0;
	gProcStruct.refcon = 0L;
}


//////////
//
// QTCmpr_FilterProc
// Filter events for a standard modal dialog box. 
//
//////////

static PASCAL_RTN Boolean QTCmpr_FilterProc (DialogPtr theDialog, EventRecord *theEvent, short *theItemHit, long theRefCon)
{
#pragma unused(theItemHit, theRefCon)
	Boolean			myEventHandled = false;
	WindowRef		myEventWindow = NULL;
	WindowRef		myDialogWindow = NULL;

#if TARGET_API_MAC_CARBON
	myDialogWindow = GetDialogWindow(theDialog);
#else
	myDialogWindow = theDialog;
#endif
	
	switch (theEvent->what) {
		case updateEvt:
			// update the specified window, if it's behind the modal dialog box
			myEventWindow = (WindowRef)theEvent->message;
			if ((myEventWindow != NULL) && (myEventWindow != myDialogWindow)) {
#if TARGET_OS_MAC
				QTFrame_HandleEvent(theEvent);
#endif
				myEventHandled = false;		// so sayeth IM
			}
			break;
	}
	
	return(myEventHandled);
}


//////////
//
// QTCmpr_ButtonProc
// Handle item selections in the standard image compression dialog box.
//
// The theParams parameter is the component instance of the standard image compression
// dialog component. Also, the theRefCon parameter is a handle to our pixel map.
//
//////////

static PASCAL_RTN short QTCmpr_ButtonProc (DialogPtr theDialog, short theItemHit, void *theParams, long theRefCon)
{
#pragma unused(theDialog)
	// in this sample code, we'll have the settings revert to their default values
	// when the user clicks on the custom button
	if (theItemHit == scCustomItem)
		SCDefaultPixMapSettings(theParams, (PixMapHandle)theRefCon, false);

	// always return the item passed in
	return(theItemHit);
}


//////////
//
// QTCmpr_CompletionProc
// Handle the completion of an asynchronous compression sequence.
//
// The theRefCon parameter is a pointer to a variable of type OSErr; we want to move
// the value in theResult into that variable.
//
//////////

static PASCAL_RTN void QTCmpr_CompletionProc (OSErr theResult, short theFlags, long theRefCon)
{
	OSErr		*myErrPtr = NULL;
	
	if (theFlags & codecCompletionDest) {
		myErrPtr = (OSErr *)theRefCon;
		if (myErrPtr != NULL)
			*myErrPtr = theResult;
	}
}
