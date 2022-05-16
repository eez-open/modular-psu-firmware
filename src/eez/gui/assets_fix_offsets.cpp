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

#include <eez/gui/assets.h>

#include <eez/gui/gui.h>

#include <eez/gui/widgets/containers/container.h>
#include <eez/gui/widgets/containers/grid.h>
#include <eez/gui/widgets/containers/list.h>
#include <eez/gui/widgets/containers/select.h>

#include <eez/gui/widgets/button.h>
#include <eez/gui/widgets/multiline_text.h>
#include <eez/gui/widgets/scroll_bar.h>
#include <eez/gui/widgets/text.h>
#include <eez/gui/widgets/toggle_button.h>
#include <eez/gui/widgets/up_down.h>

#include <eez/flow/flow_defs_v3.h>
#include <eez/flow/components/switch.h>
#include <eez/flow/components/set_variable.h>

namespace eez {
namespace gui {

template<typename T>
void fixOffset(AssetsPtrImpl<T> &ptr, void *assets) {
    if (ptr.offset) {
        ptr.offset += (uint8_t *)assets + 4 - MEMORY_BEGIN;
    }
}

template<typename T>
void fixOffset(T *&ptr, Assets *assets) {
    if (ptr) {
        ptr = reinterpret_cast<T *>((uint8_t *)ptr + (uint32_t)assets + 4);
    }
}

template<typename T>
void fixOffset(ListOfAssetsPtr<T> &list, Assets *assets) {
    fixOffset(list.items, assets);
    AssetsPtr<T> *items = (AssetsPtr<T> *)list.items;
    for (uint32_t i = 0; i < list.count; i++) {
        fixOffset(items[i], assets);
    }
}

template<typename T>
void fixOffset(ListOfFundamentalType<T> &list, Assets *assets) {
    fixOffset(list.items, assets);
}

void fixOffsets(Assets *assets, ListOfAssetsPtr<Widget> &widgets);
void fixOffsets(Assets *assets, Widget *widget);
void fixOffsets(Assets *assets, ListOfAssetsPtr<Value> &values);
void fixOffsets(Assets *assets, ListOfAssetsPtr<Language> &languages);
void fixOffsets(Assets *assets, Value &value);

void fixOffsets(Assets *assets) {
	fixOffset(assets->pages, assets);
    for (uint32_t i = 0; i < assets->pages.count; i++) {
        fixOffsets(assets, assets->pages[i]->widgets);
    }

	fixOffset(assets->styles, assets);

	fixOffset(assets->fonts, assets);
    for (uint32_t i = 0; i < assets->fonts.count; i++) {
        auto font = assets->fonts[i];
        fixOffset(font->groups, assets);
        fixOffset(font->glyphs, assets);
    }

    fixOffset(assets->bitmaps, assets);

    if (assets->colorsDefinition) {
        fixOffset(assets->colorsDefinition, assets);
        auto colorsDefinition = static_cast<Colors *>(assets->colorsDefinition);
        auto &themes = colorsDefinition->themes;
        fixOffset(themes, assets);
        for (uint32_t i = 0; i < themes.count; i++) {
            auto theme = themes[i];
            fixOffset(theme->name, assets);
            fixOffset(theme->colors, assets);
        }
        fixOffset(colorsDefinition->colors, assets);
    }

	fixOffset(assets->actionNames, assets);

	fixOffset(assets->variableNames, assets);

    if (assets->flowDefinition) {
        fixOffset(assets->flowDefinition, assets);
        auto flowDefinition = static_cast<FlowDefinition *>(assets->flowDefinition);

        fixOffset(flowDefinition->flows, assets);
        for (uint32_t i = 0; i < flowDefinition->flows.count; i++) {
            auto flow = flowDefinition->flows[i];

            fixOffset(flow->components, assets);
            for (uint32_t i = 0; i < flow->components.count; i++) {
                auto component = flow->components[i];

                using namespace eez::flow;
                using namespace eez::flow::defs_v3;

                switch (component->type) {
                case COMPONENT_TYPE_SWITCH_ACTION:
                    fixOffset(((SwitchActionComponent *)component)->tests, assets);
                    break;
                case COMPONENT_TYPE_SET_VARIABLE_ACTION:
                    {
                        auto setVariableActionComponent = (SetVariableActionComponent *)component;
                        fixOffset(setVariableActionComponent->entries, assets);
                        for (uint32_t entryIndex = 0; entryIndex < setVariableActionComponent->entries.count; entryIndex++) {
                            auto entry = setVariableActionComponent->entries[entryIndex];
                            fixOffset(entry->variable, assets);
                            fixOffset(entry->value, assets);
                        }
                    }
                    break;
                }

                fixOffset(component->inputs, assets);
                fixOffset(component->properties, assets);

                fixOffset(component->outputs, assets);
                for (uint32_t i = 0; i < component->outputs.count; i++) {
                    auto componentOutput = component->outputs[i];
                    fixOffset(componentOutput->connections, assets);
                }
            }

            fixOffsets(assets, flow->localVariables);
            fixOffset(flow->componentInputs, assets);
            fixOffset(flow->widgetDataItems, assets);
            fixOffset(flow->widgetActions, assets);
        }

        fixOffsets(assets, flowDefinition->constants);
        fixOffsets(assets, flowDefinition->globalVariables);
    }

    fixOffsets(assets, assets->languages);
}

void fixOffsets(Assets *assets, ListOfAssetsPtr<Widget> &widgets) {
    fixOffset(widgets, assets);
    for (uint32_t i = 0; i < widgets.count; i++) {
        fixOffsets(assets, widgets[i]);
    }
}

void fixOffsets(Assets *assets, Widget *widget) {
    switch (widget->type) {
    case WIDGET_TYPE_CONTAINER:
        fixOffsets(assets, ((ContainerWidget *)widget)->widgets);
        break;
    case WIDGET_TYPE_GRID:
        {
            auto gridWidget = (GridWidget *)widget;
            if (gridWidget->itemWidget) {
                fixOffset(gridWidget->itemWidget, assets);
                fixOffsets(assets, static_cast<Widget *>(gridWidget->itemWidget));
            }
        }
        break;
    case WIDGET_TYPE_LIST:
        {
            auto listWidget = (ListWidget *)widget;
            if (listWidget->itemWidget) {
                fixOffset(listWidget->itemWidget, assets);
                fixOffsets(assets, static_cast<Widget *>(listWidget->itemWidget));
            }
        }
        break;
    case WIDGET_TYPE_SELECT:
        fixOffsets(assets, ((SelectWidget *)widget)->widgets);
        break;
    case WIDGET_TYPE_BUTTON:
        fixOffset(((ButtonWidget*)widget)->text, assets);
        break;
    case WIDGET_TYPE_MULTILINE_TEXT:
        fixOffset(((MultilineTextWidget*)widget)->text, assets);
        break;
    case WIDGET_TYPE_SCROLL_BAR:
        fixOffset(((ScrollBarWidget*)widget)->leftButtonText, assets);
        fixOffset(((ScrollBarWidget*)widget)->rightButtonText, assets);
        break;
    case WIDGET_TYPE_TEXT:
        fixOffset(((TextWidget*)widget)->text, assets);
        break;
    case WIDGET_TYPE_TOGGLE_BUTTON:
        fixOffset(((ToggleButtonWidget*)widget)->text1, assets);
        fixOffset(((ToggleButtonWidget*)widget)->text2, assets);
        break;
    case WIDGET_TYPE_UP_DOWN:
        fixOffset(((UpDownWidget*)widget)->downButtonText, assets);
        fixOffset(((UpDownWidget*)widget)->upButtonText, assets);
        break;
    }
}

void fixOffsets(Assets *assets, ListOfAssetsPtr<Value> &values) {
    fixOffset(values, assets);

    for (uint32_t i = 0; i < values.count; i++) {
        auto value = values[i];
        fixOffsets(assets, *value);
    }
}

void fixOffsets(Assets *assets, ListOfAssetsPtr<Language> &languages) {
    fixOffset(languages, assets);

    for (uint32_t i = 0; i < languages.count; i++) {
        auto language = languages[i];
        fixOffset(language->languageID, assets);
        fixOffset(language->translations, assets);
    }
}

void fixOffsets(Assets *assets, Value &value) {
    if (value.getType() == VALUE_TYPE_STRING) {
    	AssetsPtr<const char> assetsPtr;

#if _WIN64 || __x86_64__ || __ppc64__
        assetsPtr.offset = value.uint32Value;
#else
        assetsPtr = (const char *)value.uint32Value;
#endif

        fixOffset(assetsPtr, assets);
        value.strValue = (const char *)assetsPtr;
    } else if (value.getType() == VALUE_TYPE_ARRAY) {
    	AssetsPtr<ArrayValue> assetsPtr;

#if _WIN64 || __x86_64__ || __ppc64__
        assetsPtr.offset = value.uint32Value;
#else
        assetsPtr = (ArrayValue *)value.uint32Value;
#endif

        fixOffset(assetsPtr, assets);
        value.arrayValue = (eez::gui::ArrayValue *)(assetsPtr);
        for (uint32_t i = 0; i < value.arrayValue->arraySize; i++) {
            fixOffsets(assets, value.arrayValue->values[i]);
        }
    }
}

} // namespace gui
} // namespace eez
