/* ***** BEGIN MIV LICENSE BLOCK *****
 * Version: MIV 1.0
 *
 * This file is part of the "MIV" license.
 *
 * Rules of this license:
 * - This code may be reused in other free software projects (free means the end user does not have to pay anything to use it).
 * - This code may be reused in other non free software projects. 
 *     !! For this rule to apply you will grant or provide the below mentioned author unlimited free access/license to the:
 *         - binary program of the non free software project which uses this code. By this we mean a full working version.
 *         - One piece of the hardware using this code. For free at no costs to the author. 
 *         - 1% of the netto world wide sales.
 * - When you use this code always leave this complete license block in the file.
 * - When you create binaries (executable or library) based on this source you 
 *     need to provide a copy of this source online publicaly accessable.
 * - When you make modifications to this source file you will keep this license block complete.
 * - When you make modifications to this source file you will send a copy of the new file to 
 *     the author mentioned in this license block. These rules will also apply to the new file.
 * - External packages used by this source might have a different license which you should comply with.
 *
 * Latest version of this license can be found at http://www.1st-setup.nl
 *
 * Author: Michel Verbraak (info@1st-setup.nl)
 * Website: http://www.1st-setup.nl
 * email: info@1st-setup.nl
 *
 *
 * ***** END MIV LICENSE BLOCK *****/

#include "font.h"
#include "globalFunctions.h"
#include "lists.h"

int freeTypeLibraryInitialized = 0;

FT_Library freeTypeLibrary = NULL;
FT_Face fontFace = NULL;
struct SIMPLELISTITEM_T *fontList = NULL;
struct SIMPLELISTITEM_T *fontListEnd = NULL;

int initFreeTypeLibrary()
{
	if (freeTypeLibraryInitialized == 1) return 0;

	int error = FT_Init_FreeType( &freeTypeLibrary );
	if (error != 0) { 
		logInfo(LOG_FREETYPE, "Error FT_Init_FreeType. (error=%d)\n", error);
		return -1;
	}

	freeTypeLibraryInitialized = 1; 
	return 0;
}

FT_Face loadFontFace(char *fontFile, int index)
{
	initFreeTypeLibrary();
	FT_Face result = NULL;

	int error = FT_New_Face( freeTypeLibrary, fontFile, index, &result);
	if (error == FT_Err_Unknown_File_Format) {
		logInfo(LOG_FREETYPE, "Error font file found but font format unsupported.\n");
		return NULL;
	} 
	else {
		if (error) { 
			logInfo(LOG_FREETYPE, "Error opening fontfile.(error=%d)\n", error);
			return NULL;
		}
	}

	return result;
} 

struct fontListItem *fontExists(FT_Face fontFace, FT_UInt size)
{
	struct SIMPLELISTITEM_T *tmpItem;

	tmpItem = fontList;

	struct fontListItem *tmpFontListItem = NULL;

	if (tmpItem != NULL) {
		tmpFontListItem = tmpItem->object;
	}

	while ((tmpItem != NULL) && (tmpFontListItem->fontFace != fontFace) && (tmpFontListItem->size != size)) {
		tmpItem = tmpItem->next;
		if (tmpItem != NULL) {
			tmpFontListItem = tmpItem->object;
		}
	}

	if ((tmpItem != NULL) && (tmpFontListItem != NULL) && (tmpFontListItem->fontFace == fontFace)  && (tmpFontListItem->size == size)) {
		return tmpFontListItem;
	}

	return NULL;
}

struct SIMPLELISTITEM_T *addToFontList(struct fontListItem *newItem)
{
	if (fontList == NULL) {
		fontList = createSimpleListItem(newItem);
		fontListEnd = fontList;
		return fontList;
	}

	addObjectToSimpleList(fontListEnd, newItem);
	fontListEnd = fontListEnd->next;
	return fontListEnd;
}

static void showVGErrorStr(VGErrorCode vg_error, char *userStr)
{
		if (vg_error == VG_BAD_HANDLE_ERROR) logInfo(LOG_OSD, "%s: VG_BAD_HANDLE_ERROR\n", userStr);
		if (vg_error == VG_ILLEGAL_ARGUMENT_ERROR) logInfo(LOG_OSD, "%s: VG_ILLEGAL_ARGUMENT_ERROR\n", userStr);
		if (vg_error == VG_OUT_OF_MEMORY_ERROR) logInfo(LOG_OSD, "%s: VG_OUT_OF_MEMORY_ERROR\n", userStr);
		if (vg_error == VG_PATH_CAPABILITY_ERROR) logInfo(LOG_OSD, "%s: VG_PATH_CAPABILITY_ERROR\n", userStr);
		if (vg_error == VG_UNSUPPORTED_IMAGE_FORMAT_ERROR) logInfo(LOG_OSD, "%s: VG_UNSUPPORTED_IMAGE_FORMAT_ERROR\n", userStr);
		if (vg_error == VG_UNSUPPORTED_PATH_FORMAT_ERROR) logInfo(LOG_OSD, "%s: VG_UNSUPPORTED_PATH_FORMAT_ERROR\n", userStr);
		if (vg_error == VG_IMAGE_IN_USE_ERROR) logInfo(LOG_OSD, "%s: VG_IMAGE_IN_USE_ERROR\n", userStr);
		if (vg_error == VG_NO_CONTEXT_ERROR) logInfo(LOG_OSD, " %s: VG_NO_CONTEXT_ERROR\n", userStr);

		logInfo(LOG_OSD, "%s: 0x%x\n", userStr, vg_error);
}

int createFont(FT_Face fontFace, FT_UInt size)
{
	initFreeTypeLibrary();

	VGfloat glyphOrigin[2];
	VGfloat escapement[2];

	FT_Stroker fontStroker;
	int error = FT_Stroker_New(freeTypeLibrary, &fontStroker);
	if (error) {
		logInfo(LOG_FREETYPE, "Error FT_Stroker_New.(error=%d)\n", error);
		return -1;
	}

	FT_Stroker_Set(fontStroker,
		 2*64.0f,  // Need to get the right value based on size.
		 FT_STROKER_LINECAP_ROUND,
		 FT_STROKER_LINEJOIN_ROUND,
		 0);

	VGFont tmpFont;
	struct fontListItem *tmpFontListItem = fontExists(fontFace, size);
	if (tmpFontListItem == NULL) {
		tmpFont = vgCreateFont(fontFace->num_glyphs);
		if (tmpFont == VG_INVALID_HANDLE) {
			logInfo(LOG_FREETYPE, "Error could not create vgCreateFont.\n");
			return -1;
		}

		tmpFontListItem = malloc(sizeof(struct fontListItem));
		tmpFontListItem->fontFace = fontFace;
		tmpFontListItem->size = size;
		tmpFontListItem->font = tmpFont;
		addToFontList(tmpFontListItem);
	}
	else {
		return 0;
	}

	error = FT_Set_Char_Size( fontFace, /* handle to face object */
					0, /* char_width in 1/64th of points */
					size*64, /* char_height in 1/64th of points */
					72, /* horizontal device resolution */
					72 ); /* vertical device resolution */

	if (error) {
		logInfo(LOG_FREETYPE, "Error FT_Set_Char_Size.(error=%d)\n", error);
		return -1;
	}

	int index;
	int counter = 0;
	FT_UInt charIndex;
	VGImage image = VG_INVALID_HANDLE;
	VGImage softenedImage;
	VGfloat blustStdDev;
	int padding;
	int image_width;
	int image_height;
	VGErrorCode vg_error;
	FT_Glyph glyph;

	logInfo(LOG_FREETYPE, "This font contains %ld glyphs.\n", fontFace->num_glyphs);

	for (index = 32; (index < 256) && (counter < fontFace->num_glyphs); index++) {
		counter++;
		charIndex = FT_Get_Char_Index(fontFace, index);

		logInfo(LOG_FREETYPE, "index=0x%x, charIndex=0x%x\n", index, charIndex);

		escapement[0] = 0;
		escapement[1] = 0;

		if (charIndex == 0) {
			vgSetGlyphToImage(tmpFont, index, VG_INVALID_HANDLE, escapement, escapement);
			logInfo(LOG_FREETYPE, "charindex== 0\n");
			continue;
		}

		error = FT_Load_Glyph(fontFace, charIndex, FT_LOAD_NO_HINTING);
		if (error) {
			vgSetGlyphToImage(tmpFont, index, VG_INVALID_HANDLE, escapement, escapement);
			logInfo(LOG_FREETYPE, "Error FT_Load_Glyph (error:%d)\n", error);
			continue;
		}

		error = FT_Get_Glyph(fontFace->glyph, &glyph);
		if (error) {
			vgSetGlyphToImage(tmpFont, index, VG_INVALID_HANDLE, escapement, escapement);
			logInfo(LOG_FREETYPE, "Error FT_Get_Glyph (error:%d)\n", error);
			continue;
		}

/*
		error = FT_Glyph_StrokeBorder(&glyph, fontStroker, 0, 1);
		if (error) {
			FT_Done_Glyph(glyph);
			vgSetGlyphToImage(tmpFont, index, VG_INVALID_HANDLE, escapement, escapement);
			logInfo(LOG_FREETYPE, "Error FT_Glyph_StrokeBorder (error:%d)\n", error);
			continue;
		}
*/

		error = FT_Glyph_To_Bitmap(&glyph, FT_RENDER_MODE_NORMAL, NULL, 1);
		if (error) {
			FT_Done_Glyph(glyph);
			vgSetGlyphToImage(tmpFont, index, VG_INVALID_HANDLE, escapement, escapement);
			logInfo(LOG_FREETYPE, "Error FT_Glyph_To_Bitmap (error:%d)\n", error);
			continue;
		}

		FT_BitmapGlyph bitGlyph = (FT_BitmapGlyph)glyph;
		FT_Bitmap bitmap = bitGlyph->bitmap;

		if (bitmap.width > 0 && bitmap.rows > 0) {
			blustStdDev = 0.6;
			padding = (3*blustStdDev + 0.5);
			image_width = bitmap.width + padding*2;
			image_height = bitmap.rows + padding*2;

			image = vgCreateImage(VG_A_8, image_width, image_height, VG_IMAGE_QUALITY_NONANTIALIASED);
			if (image == VG_INVALID_HANDLE) {
				FT_Done_Glyph(glyph);
				vgSetGlyphToImage(tmpFont, index, VG_INVALID_HANDLE, escapement, escapement);
				logInfo(LOG_FREETYPE, "vgCreateImage (error:%d)\n", vgGetError());
				continue;
			}

			if (bitmap.pitch > 0) {
				vgImageSubData(image,
					 bitmap.buffer + bitmap.pitch*(bitmap.rows-1),
					 -bitmap.pitch,
					 VG_A_8,
					 padding,
					 padding,
					 bitmap.width,
					 bitmap.rows);
			} else {
				vgImageSubData(image,
					 bitmap.buffer,
					 bitmap.pitch,
					 VG_A_8,
					 padding,
					 padding,
					 bitmap.width,
					 bitmap.rows);
			}
			vg_error = vgGetError();
			if (vg_error) {
				vgDestroyImage(image);
				FT_Done_Glyph(glyph);
				vgSetGlyphToImage(tmpFont, index, VG_INVALID_HANDLE, escapement, escapement);
				showVGErrorStr(vg_error, "vgImageSubData");
				continue;
			}

			softenedImage = vgCreateImage(VG_A_8,
							image_width,
							image_height,
							VG_IMAGE_QUALITY_NONANTIALIASED);
			if (softenedImage == VG_INVALID_HANDLE) {
				vgDestroyImage(image);
				FT_Done_Glyph(glyph);
				vgSetGlyphToImage(tmpFont, index, VG_INVALID_HANDLE, escapement, escapement);
				logInfo(LOG_FREETYPE, "vgCreateImage (error:%d)\n", vgGetError());
				continue;
			}

			// Even out hard and soft edges
			vgGaussianBlur(softenedImage, image, blustStdDev, blustStdDev, VG_TILE_FILL);
			vg_error = vgGetError();
			if (vg_error) {
				vgDestroyImage(softenedImage);
				vgDestroyImage(image);
				FT_Done_Glyph(glyph);
				vgSetGlyphToImage(tmpFont, index, VG_INVALID_HANDLE, escapement, escapement);
				showVGErrorStr(vg_error, "vgGaussianBlur");
				continue;
			}

			vgDestroyImage(image);
			image = softenedImage;

			glyphOrigin[0] = (VGfloat)(padding - bitGlyph->left);
			glyphOrigin[1] = (VGfloat)(padding + bitmap.rows - bitGlyph->top - 1);

		}
		else {
			logInfo(LOG_FREETYPE, "Error bitmap.width = %d, bitmap.rows = %d\n", bitmap.width, bitmap.rows);
		}

		escapement[0] = (VGfloat)((fontFace->glyph->advance.x + 32) / 64);
		escapement[1] = 0;

		vgSetGlyphToImage(tmpFont, index, image, glyphOrigin, escapement);
		vg_error = vgGetError();
		if (vg_error) {
			vgDestroyImage(softenedImage);
			vgDestroyImage(image);
			FT_Done_Glyph(glyph);
			vgSetGlyphToImage(tmpFont, index, VG_INVALID_HANDLE, escapement, escapement);
			showVGErrorStr(vg_error, "vgSetGlyphToImage");
			continue;
		}
		logInfo(LOG_FREETYPE, "Create glyph %d.\n", index);

		FT_Done_Glyph(glyph);

		if (image != VG_INVALID_HANDLE) {
			vgDestroyImage(image);
		}
	}

	return 0;
}



