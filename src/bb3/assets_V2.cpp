/*
 * EEZ Modular Firmware
 * Copyright (C) 2015-present, Envox d.o.o.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <eez/core/debug.h>

#include <eez/gui/gui.h>

#include <eez/gui/widgets/containers/app_view.h>
#include <eez/gui/widgets/containers/container.h>
#include <eez/gui/widgets/containers/grid.h>
#include <eez/gui/widgets/containers/layout_view.h>
#include <eez/gui/widgets/containers/list.h>
#include <eez/gui/widgets/containers/select.h>

#include <eez/gui/widgets/bar_graph.h>
#include <eez/gui/widgets/bitmap.h>
#include <eez/gui/widgets/button.h>
#include <eez/gui/widgets/button_group.h>
#include <eez/gui/widgets/display_data.h>
#include <eez/gui/widgets/list_graph.h>
#include <eez/gui/widgets/multiline_text.h>
#include <eez/gui/widgets/scroll_bar.h>
#include <eez/gui/widgets/text.h>
#include <eez/gui/widgets/toggle_button.h>
#include <eez/gui/widgets/up_down.h>

namespace eez {

using namespace gui;

////////////////////////////////////////////////////////////////////////////////

struct AssetsV2 {
	uint32_t document;
	uint32_t styles;
	uint32_t fontsData;
	uint32_t bitmapsData;
	uint32_t colorsData;
	uint32_t actionNames;
	uint32_t dataItemNames;
};

////////////////////////////////////////////////////////////////////////////////

struct AssetsV3List {
	uint32_t count;
	uint32_t first;
};

////////////////////////////////////////////////////////////////////////////////

static void convertPages();
static void convertStyles();
static void convertFonts();
static void convertBitmaps();
static void convertColorsDefinition();
static void convertActionNames();
static void convertVariableNames();
static void convertFlowDefinition();

////////////////////////////////////////////////////////////////////////////////

static AssetsV2 *g_assetsV2;
static Assets *g_assetsV3;
static uint32_t g_offsetV3;

static uint32_t g_offsetAdjust;
#define OFFSET_ADJUST(x) *((int32_t *)(x)) = (int32_t)((uint8_t *)(*(x) + g_offsetAdjust) - (uint8_t *)(x));

////////////////////////////////////////////////////////////////////////////////

void convertV2toV3(Assets *assetsV2, Assets *assetsV3) {
	assetsV3->projectMajorVersion = PROJECT_VERSION_V3;
	assetsV3->projectMinorVersion = 0;
	assetsV3->assetsType = ASSETS_TYPE_FIRMWARE;
	assetsV3->external = assetsV2->external;

	g_assetsV2 = (AssetsV2 *)&assetsV2->settings;
	g_assetsV3 = assetsV3;
	g_offsetV3 = sizeof(Assets) - 4;

	g_offsetAdjust = (uint8_t *)assetsV3 + 4 - (uint8_t *)(sizeof(void*) == 4 ? 0 : MEMORY_BEGIN);

	convertPages();
	convertStyles();
	convertFonts();
	convertBitmaps();
	convertColorsDefinition();
	convertActionNames();
	convertVariableNames();
	convertFlowDefinition();
}

////////////////////////////////////////////////////////////////////////////////

#define ADD_V3_OFFSET(offset) g_offsetV3 = (((g_offsetV3 + (offset)) + 3) / 4) * 4

////////////////////////////////////////////////////////////////////////////////

struct WidgetList {
	uint32_t count;
	uint32_t first;
};

struct Document {
	WidgetList pages;
};

struct WidgetV2 {
	uint8_t type;
	int16_t data;
	int16_t action;
	int16_t x;
	int16_t y;
	int16_t width;
	int16_t height;
	uint16_t style;
	uint32_t specific;
};

struct ContainerWidgetV2 {
	WidgetList widgets;
	int16_t overlay;
	uint8_t flags;
};

struct ListWidgetV2 {
	uint8_t listType;
	uint32_t itemWidget;
	uint8_t gap;
};

struct GridWidgetV2 {
	uint8_t gridFlow;
	uint32_t itemWidget;
};

struct SelectWidgetV2 {
	WidgetList widgets;
};

struct DisplayDataWidgetV2 {
	uint8_t displayOption;
};

struct TextWidgetV2 {
	uint32_t text;
	uint8_t flags;
};

struct MultilineTextWidgetV2 {
	uint32_t text;
	int16_t firstLineIndent;
	int16_t hangingIndent;
};

struct RectangleFlagsV2 {
	unsigned invertColors : 1;
	unsigned ignoreLuminosity : 1;
};

struct RectangleWidgetV2 {
	RectangleFlagsV2 flags;
};

struct ButtonGroupWidgetV2 {
	uint16_t selectedStyle;
};

struct BarGraphWidgetV2 {
	uint8_t orientation;
	uint16_t textStyle;
	int16_t line1Data;
	uint16_t line1Style;
	int16_t line2Data;
	uint16_t line2Style;
};

struct UpDownWidgetV2 {
	uint16_t buttonsStyle;
	uint32_t downButtonText;
	uint32_t upButtonText;
};

struct ListGraphWidgetV2 {
	int16_t dwellData;
	int16_t y1Data;
	uint16_t y1Style;
	int16_t y2Data;
	uint16_t y2Style;
	int16_t cursorData;
	uint16_t cursorStyle;
};

struct ButtonWidgetV2 {
	uint32_t text;
	int16_t enabled;
	uint16_t disabledStyle;
};

struct ToggleButtonWidgetV2 {
	uint32_t text1;
	uint32_t text2;
};

struct BitmapWidgetV2 {
	int8_t bitmap;
};

struct LayoutViewWidgetV2 {
	int16_t layout;
	int16_t context;
};

struct ScrollBarWidgetV2 {
	uint16_t thumbStyle;
	uint16_t buttonsStyle;
	uint32_t leftButtonText;
	uint32_t rightButtonText;
};

static void convertWidgetList(WidgetList &widgetList, AssetsV3List *listV3);
static uint32_t convertWidget(WidgetV2 &widgetV2);
static void convertText(uint32_t textOffset, uint32_t *location);

static Document *g_documentV2;

static void convertPages() {
	g_documentV2 = (Document *)((uint8_t *)g_assetsV2 + g_assetsV2->document);
	convertWidgetList((WidgetList &)g_documentV2->pages, (AssetsV3List *)&g_assetsV3->pages);
}

static void convertWidgetList(WidgetList &widgetList, AssetsV3List *listV3) {
	listV3->count = widgetList.count;
	listV3->first = g_offsetV3;

	uint32_t *list = (uint32_t *)((uint8_t *)g_assetsV3 + 4 + g_offsetV3);

	ADD_V3_OFFSET(widgetList.count * sizeof(uint32_t));

	WidgetV2 *widgets = (WidgetV2 *)((uint8_t *)g_documentV2 + widgetList.first);
	for (uint32_t i = 0; i < widgetList.count; i++) {
		list[i] = convertWidget(widgets[i]);
	}

	OFFSET_ADJUST(&listV3->first);
	for (uint32_t i = 0; i < widgetList.count; i++) {
		OFFSET_ADJUST(list + i);
	}
}

static uint32_t convertWidget(WidgetV2 &widgetV2) {
	auto savedOffset = g_offsetV3;

	Widget &widgetV3 = *(Widget *)((uint8_t *)g_assetsV3 + 4 + g_offsetV3);

	widgetV3.type = widgetV2.type;
	widgetV3.data = widgetV2.data;
    widgetV3.visible = DATA_ID_NONE;
	widgetV3.action = widgetV2.action;
	widgetV3.x = widgetV2.x;
	widgetV3.y = widgetV2.y;
	widgetV3.width = widgetV2.width;
	widgetV3.height = widgetV2.height;
	widgetV3.style = widgetV2.style;
    widgetV3.flags = 0;
    widgetV3.timeline.count = 0;

	auto specificV2 = (uint8_t *)g_documentV2 + widgetV2.specific;

	if (widgetV2.type == WIDGET_TYPE_CONTAINER) {
		auto &widgetV2Specific = *(ContainerWidgetV2 *)specificV2;
		auto &widgetV3Specific = (ContainerWidget &)widgetV3;

		ADD_V3_OFFSET(sizeof(ContainerWidget));

		convertWidgetList(widgetV2Specific.widgets, (AssetsV3List *)&widgetV3Specific.widgets);
		widgetV3Specific.flags = widgetV2Specific.flags;
		widgetV3Specific.overlay = widgetV2Specific.overlay;
        widgetV3Specific.layout = CONTAINER_WIDGET_LAYOUT_STATIC;

		return savedOffset;
	}
	
	if (widgetV2.type == WIDGET_TYPE_LIST) {
		auto &widgetV2Specific = *(ListWidgetV2 *)specificV2;
		auto &widgetV3Specific = (ListWidget &)widgetV3;

		ADD_V3_OFFSET(sizeof(ListWidget));

		*((uint32_t *)(&widgetV3Specific.itemWidget)) = convertWidget((WidgetV2 &)*((uint8_t *)g_documentV2 + widgetV2Specific.itemWidget));
		widgetV3Specific.listType = widgetV2Specific.listType;
		widgetV3Specific.gap = widgetV2Specific.gap;

		return savedOffset;
	}
	
	if (widgetV2.type == WIDGET_TYPE_GRID) {
		auto &widgetV2Specific = *(GridWidgetV2 *)specificV2;
		auto &widgetV3Specific = (GridWidget &)widgetV3;

		*((uint32_t *)(&widgetV3Specific.itemWidget)) = convertWidget((WidgetV2 &)*((uint8_t *)g_documentV2 + widgetV2Specific.itemWidget));
		widgetV3Specific.gridFlow = widgetV2Specific.gridFlow;

		ADD_V3_OFFSET(sizeof(GridWidget));

		return savedOffset;
	}
	
	if (widgetV2.type == WIDGET_TYPE_SELECT) {
		auto &widgetV2Specific = *(SelectWidgetV2 *)specificV2;
		auto &widgetV3Specific = (SelectWidget &)widgetV3;

		ADD_V3_OFFSET(sizeof(SelectWidget));

		convertWidgetList(widgetV2Specific.widgets, (AssetsV3List *)&widgetV3Specific.widgets);
	
		return savedOffset;
	}
	
	if (widgetV2.type == WIDGET_TYPE_DISPLAY_DATA) {
		auto &widgetV2Specific = *(DisplayDataWidgetV2 *)specificV2;
		auto &widgetV3Specific = (DisplayDataWidget &)widgetV3;

		widgetV3Specific.displayOption = widgetV2Specific.displayOption;

		ADD_V3_OFFSET(sizeof(DisplayDataWidget));

		return savedOffset;
	}
	
	if (widgetV2.type == WIDGET_TYPE_TEXT) {
		auto &widgetV2Specific = *(TextWidgetV2 *)specificV2;
		auto &widgetV3Specific = (TextWidget &)widgetV3;

		ADD_V3_OFFSET(sizeof(TextWidget));

		convertText(widgetV2Specific.text, (uint32_t *)(&widgetV3Specific.text));
		widgetV3Specific.flags = widgetV2Specific.flags;
	
		return savedOffset;
	}
	
	if (widgetV2.type == WIDGET_TYPE_MULTILINE_TEXT) {
		auto &widgetV2Specific = *(MultilineTextWidgetV2 *)specificV2;
		auto &widgetV3Specific = (MultilineTextWidget &)widgetV3;

		ADD_V3_OFFSET(sizeof(MultilineTextWidget));

		convertText(widgetV2Specific.text, (uint32_t *)(&widgetV3Specific.text));
		widgetV3Specific.firstLineIndent = widgetV2Specific.firstLineIndent;
		widgetV3Specific.hangingIndent = widgetV2Specific.hangingIndent;
	
		return savedOffset;
	}
	
	if (widgetV2.type == WIDGET_TYPE_RECTANGLE) {
		auto &widgetV2Specific = *(RectangleWidgetV2 *)specificV2;
		auto &widgetV3Specific = (RectangleWidget &)widgetV3;

		widgetV3Specific.flags.invertColors = widgetV2Specific.flags.invertColors;
		widgetV3Specific.flags.ignoreLuminosity = widgetV2Specific.flags.ignoreLuminosity;

		ADD_V3_OFFSET(sizeof(RectangleWidget));

		return savedOffset;
	}
	
	if (widgetV2.type == WIDGET_TYPE_BITMAP) {
		auto &widgetV2Specific = *(BitmapWidgetV2 *)specificV2;
		auto &widgetV3Specific = (BitmapWidget &)widgetV3;

		ADD_V3_OFFSET(sizeof(BitmapWidget));

		widgetV3Specific.bitmap = widgetV2Specific.bitmap;
	
		return savedOffset;
	}
	
	if (widgetV2.type == WIDGET_TYPE_BUTTON) {
		auto &widgetV2Specific = *(ButtonWidgetV2 *)specificV2;
		auto &widgetV3Specific = (ButtonWidget &)widgetV3;

		ADD_V3_OFFSET(sizeof(ButtonWidget));

		convertText(widgetV2Specific.text, (uint32_t *)(&widgetV3Specific.text));
		widgetV3Specific.enabled = widgetV2Specific.enabled;
		widgetV3Specific.disabledStyle = widgetV2Specific.disabledStyle;

		return savedOffset;
	}
	
	if (widgetV2.type == WIDGET_TYPE_TOGGLE_BUTTON) {
		auto &widgetV2Specific = *(ToggleButtonWidgetV2 *)specificV2;
		auto &widgetV3Specific = (ToggleButtonWidget &)widgetV3;

		ADD_V3_OFFSET(sizeof(ToggleButtonWidget));

		convertText(widgetV2Specific.text1, (uint32_t *)(&widgetV3Specific.text1));
		convertText(widgetV2Specific.text2, (uint32_t *)(&widgetV3Specific.text2));
	
		return savedOffset;
	}
	
	if (widgetV2.type == WIDGET_TYPE_BUTTON_GROUP) {
		auto &widgetV2Specific = *(ButtonGroupWidgetV2 *)specificV2;
		auto &widgetV3Specific = (ButtonGroupWidget &)widgetV3;

		ADD_V3_OFFSET(sizeof(ButtonGroupWidget));

		widgetV3Specific.selectedStyle = widgetV2Specific.selectedStyle;
	
		return savedOffset;
	}
	
	if (widgetV2.type == WIDGET_TYPE_BAR_GRAPH) {
		auto &widgetV2Specific = *(BarGraphWidgetV2 *)specificV2;
		auto &widgetV3Specific = (BarGraphWidget &)widgetV3;

		ADD_V3_OFFSET(sizeof(BarGraphWidget));

		widgetV3Specific.textStyle = widgetV2Specific.textStyle;
		widgetV3Specific.line1Data = widgetV2Specific.line1Data;
		widgetV3Specific.line1Style = widgetV2Specific.line1Style;
		widgetV3Specific.line2Data = widgetV2Specific.line2Data;
		widgetV3Specific.line2Style = widgetV2Specific.line2Style;
		widgetV3Specific.orientation = widgetV2Specific.orientation;

		return savedOffset;
	}
	
	if (widgetV2.type == WIDGET_TYPE_LAYOUT_VIEW) {
		auto &widgetV2Specific = *(LayoutViewWidgetV2 *)specificV2;
		auto &widgetV3Specific = (LayoutViewWidget &)widgetV3;

		ADD_V3_OFFSET(sizeof(LayoutViewWidget));

		widgetV3Specific.layout = widgetV2Specific.layout;
		widgetV3Specific.context = widgetV2Specific.context;
		widgetV3Specific.componentIndex = 0;
	
		return savedOffset;
	}
	
	if (widgetV2.type == WIDGET_TYPE_YT_GRAPH) {
		ADD_V3_OFFSET(sizeof(Widget));

		// no specific fields

		return savedOffset;
	}
	
	if (widgetV2.type == WIDGET_TYPE_UP_DOWN) {
		auto &widgetV2Specific = *(UpDownWidgetV2 *)specificV2;
		auto &widgetV3Specific = (UpDownWidget &)widgetV3;

		ADD_V3_OFFSET(sizeof(UpDownWidget));

		convertText(widgetV2Specific.downButtonText, (uint32_t *)(&widgetV3Specific.downButtonText));
		convertText(widgetV2Specific.upButtonText, (uint32_t *)(&widgetV3Specific.upButtonText));
		widgetV3Specific.buttonsStyle = widgetV2Specific.buttonsStyle;
	
		return savedOffset;
	}
	
	if (widgetV2.type == WIDGET_TYPE_LIST_GRAPH) {
		auto &widgetV2Specific = *(ListGraphWidgetV2 *)specificV2;
		auto &widgetV3Specific = (ListGraphWidget &)widgetV3;

		ADD_V3_OFFSET(sizeof(ListGraphWidget));

		widgetV3Specific.dwellData = widgetV2Specific.dwellData;
		widgetV3Specific.y1Data = widgetV2Specific.y1Data;
		widgetV3Specific.y1Style = widgetV2Specific.y1Style;
		widgetV3Specific.y2Data = widgetV2Specific.y2Data;
		widgetV3Specific.y2Style = widgetV2Specific.y2Style;
		widgetV3Specific.cursorData = widgetV2Specific.cursorData;
		widgetV3Specific.cursorStyle = widgetV2Specific.cursorStyle;

		return savedOffset;
	}
	
	if (widgetV2.type == WIDGET_TYPE_APP_VIEW) {
		ADD_V3_OFFSET(sizeof(Widget));

		// no specific fields

		return savedOffset;
	}
	
	if (widgetV2.type == WIDGET_TYPE_SCROLL_BAR) {
		auto &widgetV2Specific = *(ScrollBarWidgetV2 *)specificV2;
		auto &widgetV3Specific = (ScrollBarWidget &)widgetV3;

		ADD_V3_OFFSET(sizeof(ScrollBarWidget));

		widgetV3Specific.thumbStyle = widgetV2Specific.thumbStyle;
		widgetV3Specific.buttonsStyle = widgetV2Specific.buttonsStyle;
		convertText(widgetV2Specific.leftButtonText, (uint32_t *)(&widgetV3Specific.leftButtonText));
		convertText(widgetV2Specific.rightButtonText, (uint32_t *)(&widgetV3Specific.rightButtonText));

		return savedOffset;
	}
	
	if (widgetV2.type == WIDGET_TYPE_PROGRESS) {
		ADD_V3_OFFSET(sizeof(Widget));

		// no specific fields

		return savedOffset;
	}
	
	if (widgetV2.type == WIDGET_TYPE_CANVAS) {
		ADD_V3_OFFSET(sizeof(Widget));

		// no specific fields

		return savedOffset;
	}

	ADD_V3_OFFSET(sizeof(Widget));

	return savedOffset;
}

static void convertText(uint32_t textOffset, uint32_t *location) {
	const char *textV2 = (const char *)((uint8_t *)g_documentV2 + textOffset);
	char *textV3 = (char *)(uint8_t *)g_assetsV3 + 4 + g_offsetV3;
	auto length = strlen(textV2) + 1;
	stringCopy(textV3, length, textV2);
	auto savedOffset = g_offsetV3;
	ADD_V3_OFFSET(length);
	*location = savedOffset;
    OFFSET_ADJUST(location);
}

////////////////////////////////////////////////////////////////////////////////

struct StyleV2 {
	uint16_t flags;
	uint16_t background_color;
	uint16_t color;
	uint16_t active_background_color;
	uint16_t active_color;
	uint16_t focus_background_color;
	uint16_t focus_color;
	uint8_t border_size_top;
	uint8_t border_size_right;
	uint8_t border_size_bottom;
	uint8_t border_size_left;
	uint16_t border_radius;
	uint16_t border_color;
	uint8_t font;
	uint8_t opacity; // 0 - 255
	uint8_t padding_top;
	uint8_t padding_right;
	uint8_t padding_bottom;
	uint8_t padding_left;
	uint8_t margin_top;
	uint8_t margin_right;
	uint8_t margin_bottom;
	uint8_t margin_left;
};

struct StyleList {
	uint32_t count;
	uint32_t first;
};

static void convertStyleList(StyleList &styleList, AssetsV3List *listV3);
static uint32_t convertStyle(StyleV2 &styleV2);

static StyleList *g_stylesV2;

static void convertStyles() {
	g_stylesV2 = (StyleList *)((uint8_t *)g_assetsV2 + g_assetsV2->styles);
	convertStyleList((StyleList &)*g_stylesV2, (AssetsV3List *)&g_assetsV3->styles);
}

static void convertStyleList(StyleList &styleList, AssetsV3List *listV3) {
	listV3->count = styleList.count;
	listV3->first = g_offsetV3;

	uint32_t *list = (uint32_t *)((uint8_t *)g_assetsV3 + 4 + g_offsetV3);

	ADD_V3_OFFSET(styleList.count * sizeof(uint32_t));

	StyleV2 *styles = (StyleV2 *)((uint8_t *)g_stylesV2 + styleList.first);
	for (uint32_t i = 0; i < styleList.count; i++) {
		list[i] = convertStyle(styles[i]);
	}

	OFFSET_ADJUST(&listV3->first);
	for (uint32_t i = 0; i < styleList.count; i++) {
		OFFSET_ADJUST(list + i);
	}
}

static uint32_t convertStyle(StyleV2 &styleV2) {
	auto savedOffset = g_offsetV3;

	Style &styleV3 = *(Style *)((uint8_t *)g_assetsV3 + 4 + g_offsetV3);

    styleV3.flags = styleV2.flags;

    styleV3.backgroundColor = styleV2.background_color;
    styleV3.color = styleV2.color;

    styleV3.activeBackgroundColor = styleV2.active_background_color;
    styleV3.activeColor = styleV2.active_color;

    styleV3.focusBackgroundColor = styleV2.focus_background_color;
    styleV3.focusColor = styleV2.focus_color;

    styleV3.borderSizeTop = styleV2.border_size_top;
    styleV3.borderSizeRight = styleV2.border_size_right;
    styleV3.borderSizeBottom = styleV2.border_size_bottom;
    styleV3.borderSizeLeft = styleV2.border_size_left;

    styleV3.borderColor = styleV2.border_color;

    styleV3.borderRadiusTLX = styleV2.border_radius;
	styleV3.borderRadiusTLY = styleV2.border_radius;
    styleV3.borderRadiusTRX = styleV2.border_radius;
	styleV3.borderRadiusTRY = styleV2.border_radius;
    styleV3.borderRadiusBLX = styleV2.border_radius;
	styleV3.borderRadiusBLY = styleV2.border_radius;
    styleV3.borderRadiusBRX = styleV2.border_radius;
	styleV3.borderRadiusBRY = styleV2.border_radius;

    styleV3.font = styleV2.font;
    styleV3.opacity = styleV2.opacity;

    styleV3.paddingTop = styleV2.padding_top;
    styleV3.paddingRight = styleV2.padding_right;
    styleV3.paddingBottom = styleV2.padding_bottom;
    styleV3.paddingLeft = styleV2.padding_left;

	styleV3.backgroundImage = 0;

	return savedOffset;
}

////////////////////////////////////////////////////////////////////////////////

static void convertFonts() {
	g_assetsV3->fonts.count = 0;
}

////////////////////////////////////////////////////////////////////////////////

struct BitmapV2 {
	int16_t w;
	int16_t h;
	int16_t bpp;
	int16_t reserved;
	const uint8_t pixels[1];
};

static void convertBitmaps() {
	auto bitmapsDataV2 = (uint32_t *)((uint8_t *)g_assetsV2 + g_assetsV2->bitmapsData);

	uint32_t count;
	auto size = 0;
	for (count = 0; bitmapsDataV2[count]; count++) {
		if (count > 0) {
			if (bitmapsDataV2[count - 1] + size != bitmapsDataV2[count]) {
				break;
			}
		}
		auto bitmapDataV2 = (BitmapV2 *)((uint8_t *)g_assetsV2 + g_assetsV2->bitmapsData + bitmapsDataV2[count]);
		size = sizeof(Bitmap) - 2 + bitmapDataV2->w * bitmapDataV2->h * bitmapDataV2->bpp / 8;
	}

	auto listV3 = (AssetsV3List *)&g_assetsV3->bitmaps;
	listV3->count = count;
	listV3->first = g_offsetV3;

	uint32_t *list = (uint32_t *)((uint8_t *)g_assetsV3 + 4 + g_offsetV3);

	ADD_V3_OFFSET(listV3->count * sizeof(uint32_t));

	for (uint32_t i = 0; i < count; i++) {
		list[i] = g_offsetV3;
		auto bitmapDataV2 = (BitmapV2 *)((uint8_t *)g_assetsV2 + g_assetsV2->bitmapsData + bitmapsDataV2[i]);
		auto bitmapDataV3 = (uint8_t *)g_assetsV3 + 4 + g_offsetV3;
		auto size = sizeof(Bitmap) - 2 + bitmapDataV2->w * bitmapDataV2->h * bitmapDataV2->bpp / 8;
		memcpy(bitmapDataV3, bitmapDataV2, size);
		ADD_V3_OFFSET(size);
	}

	OFFSET_ADJUST(&listV3->first);
	for (uint32_t i = 0; i < count; i++) {
		OFFSET_ADJUST(list + i);
	}
}

////////////////////////////////////////////////////////////////////////////////

static void convertColorsDefinition() {
	// we only support conversion of MP script resource file which contains no colors definition
	*((uint32_t *)&g_assetsV3->colorsDefinition) = 0;
}

////////////////////////////////////////////////////////////////////////////////

struct NameList {
	uint32_t count;
	uint32_t first;
};

static void convertNameList(NameList &styleList, AssetsV3List *listV3);
static void convertName(uint32_t nameOffset, uint32_t *location);

static NameList *g_nameListV2;

static void convertActionNames() {
	g_nameListV2 = (NameList *)((uint8_t *)g_assetsV2 + g_assetsV2->actionNames);
	convertNameList((NameList &)*g_nameListV2, (AssetsV3List *)&g_assetsV3->actionNames);
}

static void convertVariableNames() {
	g_nameListV2 = (NameList *)((uint8_t *)g_assetsV2 + g_assetsV2->dataItemNames);
	convertNameList((NameList &)*g_nameListV2, (AssetsV3List *)&g_assetsV3->variableNames);
}

static void convertNameList(NameList &nameList, AssetsV3List *listV3) {
	listV3->count = nameList.count;
	listV3->first = g_offsetV3;

	uint32_t *list = (uint32_t *)((uint8_t *)g_assetsV3 + 4 + g_offsetV3);

	ADD_V3_OFFSET(nameList.count * sizeof(uint32_t));

	uint32_t *names = (uint32_t *)((uint8_t *)g_nameListV2 + nameList.first);
	for (uint32_t i = 0; i < nameList.count; i++) {
		convertName(names[i], list + i);
	}

	OFFSET_ADJUST(&listV3->first);
}

static void convertName(uint32_t nameOffset, uint32_t *location) {
	const char *nameV2 = (const char *)((uint8_t *)g_nameListV2 + nameOffset);
	char *nameV3 = (char *)(uint8_t *)g_assetsV3 + 4 + g_offsetV3;
	auto length = strlen(nameV2) + 1;
	stringCopy(nameV3, length, nameV2);
	auto savedOffset = g_offsetV3;
	ADD_V3_OFFSET(length);
	*location = savedOffset;
    OFFSET_ADJUST(location);
}

////////////////////////////////////////////////////////////////////////////////

static void convertFlowDefinition() {
	// there is no flow definition in V2
	*((uint32_t *)&g_assetsV3->flowDefinition) = 0;
}

////////////////////////////////////////////////////////////////////////////////

} // namespace eez
