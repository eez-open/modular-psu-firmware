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

#pragma once

#include <type_traits>
#include <eez/gui/geometry.h>
#include <eez/memory.h>

namespace eez {
namespace gui {

#define WIDGET_TYPES \
    WIDGET_TYPE(NONE, 0) \
    WIDGET_TYPE(CONTAINER, 1) \
    WIDGET_TYPE(LIST, 2) \
    WIDGET_TYPE(GRID, 3) \
    WIDGET_TYPE(SELECT, 4) \
    WIDGET_TYPE(DISPLAY_DATA, 5) \
    WIDGET_TYPE(TEXT, 6) \
    WIDGET_TYPE(MULTILINE_TEXT, 7) \
    WIDGET_TYPE(RECTANGLE, 8) \
    WIDGET_TYPE(BITMAP, 9) \
    WIDGET_TYPE(BUTTON, 10) \
    WIDGET_TYPE(TOGGLE_BUTTON, 11) \
    WIDGET_TYPE(BUTTON_GROUP, 12) \
    WIDGET_TYPE(RESERVED, 13) \
    WIDGET_TYPE(BAR_GRAPH, 14) \
    WIDGET_TYPE(LAYOUT_VIEW, 15) \
    WIDGET_TYPE(YT_GRAPH, 16) \
    WIDGET_TYPE(UP_DOWN, 17) \
    WIDGET_TYPE(LIST_GRAPH, 18) \
    WIDGET_TYPE(APP_VIEW, 19) \
    WIDGET_TYPE(SCROLL_BAR, 20) \
    WIDGET_TYPE(PROGRESS, 21) \
    WIDGET_TYPE(CANVAS, 22) \

#define WIDGET_TYPE(NAME, ID) WIDGET_TYPE_##NAME = ID,
enum WidgetTypes {
	WIDGET_TYPES
};
#undef WIDGET_TYPE

typedef void (*EnumWidgetsCallback)(const WidgetCursor &widgetCursor);

template<typename T, bool is_void>
struct AssetsPtrImpl;

/* This template is used (on 64-bit systems) to pack asset pointers into 32-bit values.
 * All pointers are relative to MEMORY_BEGIN.
 * This way, the assets created by Studio can be used without having to fix all
 * the sizes - Studio creates 32-bit pointers that are relative to the
 * beginning of the assets, which the firmware rewrites to global pointers
 * during initialization. On a 32-bit system this works just fine, but for a
 * 64-bit system the pointers have different sizes and this breaks. By
 * inserting a 'middleman' structure that stores the pointers as a 32-bit
 * offset to MEMORY_BEGIN, we can keep the pointer sizes and initialization
 * code the same.
 */
template<typename T>
struct AssetsPtrImpl<T, false>
{
    /* Default constructors initialize to an invalid pointer */
    AssetsPtrImpl() = default;
    AssetsPtrImpl(std::nullptr_t v) {};
    /* Can accept void or T pointers. Store the offset to MEMORY_BEGIN. */
    AssetsPtrImpl(T * p) : val(reinterpret_cast<const uint8_t*>(p) - MEMORY_BEGIN) {}
    AssetsPtrImpl(void * p) : val(reinterpret_cast<const uint8_t*>(p) - MEMORY_BEGIN) {}

    /* Conversion to an int return the raw value */
    operator uint32_t() const { return val; }
    
    /* Implicit conversion to a T pointer */
    operator T*() const { return ptr(); }
    
    /* Implicit conversions to void pointers */
    operator       void*() const { return ptr(); }
    operator const void*() const { return ptr(); }

    /* Special cases for converting to (unsigned) char for pointer arithmetic */
    template<typename U, typename = typename std::enable_if<!std::is_same<typename std::remove_pointer<U>::type, T>::value>>
    operator U() const { return (U)ptr(); }
    
    /* Dereferencing operators */
          T* operator->()       { return ptr(); }
    const T* operator->() const { return ptr(); }
    
    /* Array access */
    T&       operator[](uint32_t i)       { return ptr()[i]; }
    const T& operator[](uint32_t i) const { return ptr()[i]; }
    
    /* Implement addition of ints just like 'normal' pointers */
    T* operator+(int i)      { return &ptr()[i]; }
    T* operator+(unsigned i) { return &ptr()[i]; }

    /* Calculate the pointer, return a null pointer if the value is invalid */
    T* ptr() const {
        if (val == 0xFFFFFFFF) {
            return 0;
        } else {
            return (T*)(MEMORY_BEGIN + val);
        }
    }

    uint32_t val{0xFFFFFFFF};
};

/* Template specialization for void pointers that does not allow dereferencing */
template<typename T>
struct AssetsPtrImpl<T, true>
{
    AssetsPtrImpl() = default;
    AssetsPtrImpl(std::nullptr_t) {};
    AssetsPtrImpl(T* p) : val(reinterpret_cast<const uint8_t*>(p) - MEMORY_BEGIN) {}
    operator uint32_t() const { return val; }
    operator T*() const { return reinterpret_cast<T*>(MEMORY_BEGIN + val); }

    /* Implicit conversions to convert to any pointer type */
    template<typename U> operator U() const { return (U)(T*)(*this); }
    uint32_t val{0xFFFFFFFF};
};

/* This struct chooses the type used for AssetsPtr<T> - by default it uses an AssetsPtrImpl<> */
template<typename T, uint32_t ptrSize>
struct AssetsPtrChooser
{
    using type = AssetsPtrImpl<T, std::is_void<T>::value>;
};

/* On 32-bit systems, we can just use raw pointers */
template<typename T>
struct AssetsPtrChooser<T, 4>
{
    using type = T*;
};

/* Utility typedef that delegates to AssetsPtrChooser */
template<typename T>
using AssetsPtr = typename AssetsPtrChooser<T, sizeof(void*)>::type;

struct Widget;
struct Assets;

typedef void (*FixPointersFunctionType)(Widget *widget, Assets *assets);
typedef void (*EnumFunctionType)(WidgetCursor &widgetCursor, EnumWidgetsCallback callback);
typedef void (*DrawFunctionType)(const WidgetCursor &widgetCursor);
typedef void (*OnTouchFunctionType)(const WidgetCursor &widgetCursor, Event &touchEvent);
typedef bool (*OnKeyboardFunctionType)(const WidgetCursor &widgetCursor, uint8_t key, uint8_t mod);

////////////////////////////////////////////////////////////////////////////////

struct Bitmap {
    int16_t w;
    int16_t h;
    int16_t bpp;
    int16_t reserved;
    const uint8_t pixels[1];
};

////////////////////////////////////////////////////////////////////////////////

#define STYLE_FLAGS_HORZ_ALIGN_MASK 0x7
#define STYLE_FLAGS_HORZ_ALIGN_LEFT 0
#define STYLE_FLAGS_HORZ_ALIGN_RIGHT 1
#define STYLE_FLAGS_HORZ_ALIGN_CENTER 2

#define STYLE_FLAGS_VERT_ALIGN_MASK (0x7 << 3)
#define STYLE_FLAGS_VERT_ALIGN_TOP (0 << 3)
#define STYLE_FLAGS_VERT_ALIGN_BOTTOM (1 << 3)
#define STYLE_FLAGS_VERT_ALIGN_CENTER (2 << 3)

#define STYLE_FLAGS_BLINK (1 << 6)

struct Style {
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
    AssetsPtr<const Style> first;
};

void StyleList_fixPointers(StyleList &styleList);

////////////////////////////////////////////////////////////////////////////////

struct Widget {
    uint8_t type;
    int16_t data;
    int16_t action;
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
    uint16_t style;
    AssetsPtr<const void> specific;
};

void Widget_fixPointers(Widget* widget);

struct WidgetList {
    uint32_t count;
    AssetsPtr<const Widget> first;
};

void WidgetList_fixPointers(WidgetList &widgetList);

////////////////////////////////////////////////////////////////////////////////

#define SHADOW_FLAG 1
#define CLOSE_PAGE_IF_TOUCHED_OUTSIDE_FLAG 2

struct PageWidget {
    WidgetList widgets;
    uint16_t overlay;
    uint8_t flags;
};

struct Document {
    WidgetList pages;
};

////////////////////////////////////////////////////////////////////////////////

struct ColorList {
    uint32_t count;
    AssetsPtr<const uint16_t> first;
};

void ColorList_fixPointers(ColorList &colorList);

struct Theme {
    AssetsPtr<const char> name;
    ColorList colors;
};

void Theme_fixPointers(Theme *theme);

struct ThemeList {
    uint32_t count;
    AssetsPtr<const Theme> first;
};

void ThemeList_fixPointers(ThemeList &themeList);

struct Colors {
    ThemeList themes;
    ColorList colors;
};

////////////////////////////////////////////////////////////////////////////////

struct NameList {
    uint32_t count;
    AssetsPtr<AssetsPtr<const char>> first;
};

struct Assets {
    AssetsPtr<Document> document;
    AssetsPtr<StyleList> styles;
    AssetsPtr<uint8_t> fontsData;
    AssetsPtr<uint8_t> bitmapsData;
    AssetsPtr<Colors> colorsData;
    AssetsPtr<NameList> actionNames;
    AssetsPtr<NameList> dataItemNames;
};

////////////////////////////////////////////////////////////////////////////////

struct WidgetStateFlags {
    unsigned active : 1;
    unsigned focused : 1;
    unsigned blinking : 1;
    unsigned enabled : 1;
};

struct WidgetState {
    uint16_t size;
    WidgetStateFlags flags;
    Value data;
};

class AppContext;

struct WidgetCursor {
    AppContext *appContext;
    const Widget *widget;
    int16_t x;
    int16_t y;
    Cursor cursor;
    WidgetState *previousState;
    WidgetState *currentState;

    WidgetCursor() 
		: appContext(nullptr)
        , widget(nullptr)
        , x(0)
        , y(0)
        , cursor(-1)
        , previousState(nullptr)
        , currentState(nullptr)
	{
    }

    WidgetCursor(
        AppContext *appContext_,
        const Widget *widget_,
        int x_,
        int y_,
        const Cursor cursor_,
        WidgetState *previousState_,
        WidgetState *currentState_
    )
        : appContext(appContext_)
        , widget(widget_)
        , x(x_)
        , y(y_)
        , cursor(cursor_)
        , previousState(previousState_)
        , currentState(currentState_)
    {
    }

    WidgetCursor &operator=(int) {
        widget = nullptr;
        cursor = -1;
        return *this;
    }

    bool operator!=(const WidgetCursor &rhs) const {
        return appContext != rhs.appContext || widget != rhs.widget || cursor != rhs.cursor;
    }

    bool operator==(const WidgetCursor &rhs) const {
        return !(*this != rhs);
    }

    operator bool() const {
        return widget != nullptr;
    }

    void nextState();
};

////////////////////////////////////////////////////////////////////////////////

void enumWidgets(AppContext* appContext, EnumWidgetsCallback callback);
void enumWidgets(WidgetCursor &widgetCursor, EnumWidgetsCallback callback);

void findWidgetStep(const WidgetCursor &widgetCursor);
WidgetCursor findWidget(AppContext* appContext, int16_t x, int16_t y, bool clicked = true);

extern OnTouchFunctionType *g_onTouchWidgetFunctions[];
extern OnKeyboardFunctionType *g_onKeyboardWidgetFunctions[];

WidgetState *nextWidgetState(WidgetState *p);
void enumWidget(WidgetCursor &widgetCursor, EnumWidgetsCallback callback);

extern bool g_isActiveWidget;
void drawWidgetCallback(const WidgetCursor &widgetCursor);

OnTouchFunctionType getWidgetTouchFunction(const WidgetCursor &widgetCursor);

uint16_t overrideStyleHook(const WidgetCursor &widgetCursor, uint16_t styleId);
uint16_t overrideStyleColorHook(const WidgetCursor &widgetCursor, const Style *style);
uint16_t overrideActiveStyleColorHook(const WidgetCursor &widgetCursor, const Style *style);

} // namespace gui
} // namespace eez
