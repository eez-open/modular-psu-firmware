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


namespace eez {
namespace gui {

void fixOffsets(Assets *assets, ListOfAssetsPtr<Widget> &widgets);
void fixOffsets(Assets *assets, Widget *widget);
void fixOffsets(Assets *assets, ListOfAssetsPtr<Value> &values);
void fixOffsets(Assets *assets, Value &value);

void fixOffsets(Assets *assets) {
	assets->pages.fixOffset(assets);
    for (uint32_t i = 0; i < assets->pages.count; i++) {
        fixOffsets(assets, assets->pages.item(i)->widgets);
    }

	assets->styles.fixOffset(assets);

	assets->fonts.fixOffset(assets);
    for (uint32_t i = 0; i < assets->fonts.count; i++) {
        auto font = assets->fonts.item(i);
        for (uint32_t glyphIndex = font->encodingStart; glyphIndex <= font->encodingEnd; glyphIndex++) {
            font->glyphs[glyphIndex - font->encodingStart].fixOffset(assets);
        }
    }

    assets->bitmaps.fixOffset(assets);

    if (assets->colorsDefinition) {
        assets->colorsDefinition.fixOffset(assets);
        auto colorsDefinition = assets->colorsDefinition.ptr();
        auto &themes = colorsDefinition->themes;
        themes.fixOffset(assets);
        for (uint32_t i = 0; i < themes.count; i++) {
            auto theme = themes.item(i);
            theme->name.fixOffset(assets);
            theme->colors.fixOffset(assets);
        }
        colorsDefinition->colors.fixOffset(assets);
    }

	assets->actionNames.fixOffset(assets);

	assets->variableNames.fixOffset(assets);

    if (assets->flowDefinition) {
        assets->flowDefinition.fixOffset(assets);
        auto flowDefinition = assets->flowDefinition.ptr();
        
        flowDefinition->flows.fixOffset(assets);
        for (uint32_t i = 0; i < flowDefinition->flows.count; i++) {
            auto flow = flowDefinition->flows.item(i);

            flow->components.fixOffset(assets);
            for (uint32_t i = 0; i < flow->components.count; i++) {
                auto component = flow->components.item(i);

                using namespace eez::flow;
                using namespace eez::flow::defs_v3;

                switch (component->type) {
                case COMPONENT_TYPE_SWITCH_ACTION:
                    ((SwitchActionComponent *)component)->tests.fixOffset(assets);
                    break;
                }

                component->inputs.fixOffset(assets);
                component->propertyValues.fixOffset(assets);

                component->outputs.fixOffset(assets);
                for (uint32_t i = 0; i < component->outputs.count; i++) {
                    auto componentOutput = component->outputs.item(i);
                    componentOutput->connections.fixOffset(assets);
                }
            }

            fixOffsets(assets, flow->localVariables);
            flow->componentInputs.fixOffset(assets);
            flow->widgetDataItems.fixOffset(assets);
            flow->widgetActions.fixOffset(assets);
        }

        fixOffsets(assets, flowDefinition->constants);
        fixOffsets(assets, flowDefinition->globalVariables);
    }
}

void fixOffsets(Assets *assets, ListOfAssetsPtr<Widget> &widgets) {
    widgets.fixOffset(assets);
    for (uint32_t i = 0; i < widgets.count; i++) {
        fixOffsets(assets, widgets.item(i));
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
                gridWidget->itemWidget.fixOffset(assets);
                fixOffsets(assets, gridWidget->itemWidget.ptr());
            }
        }
        break;
    case WIDGET_TYPE_LIST:
        {
            auto listWidget = (ListWidget *)widget;
            if (listWidget->itemWidget) {
                listWidget->itemWidget.fixOffset(assets);
                fixOffsets(assets, listWidget->itemWidget.ptr());
            }
        }
        break;
    case WIDGET_TYPE_SELECT:
        fixOffsets(assets, ((SelectWidget *)widget)->widgets);
        break;
    case WIDGET_TYPE_BUTTON:
        ((ButtonWidget*)widget)->text.fixOffset(assets);
        break;
    case WIDGET_TYPE_MULTILINE_TEXT:
        ((MultilineTextWidget*)widget)->text.fixOffset(assets);
        break;
    case WIDGET_TYPE_SCROLL_BAR:
        ((ScrollBarWidget*)widget)->leftButtonText.fixOffset(assets);
        ((ScrollBarWidget*)widget)->rightButtonText.fixOffset(assets);
        break;
    case WIDGET_TYPE_TEXT:
        ((TextWidget*)widget)->text.fixOffset(assets);
        break;
    case WIDGET_TYPE_TOGGLE_BUTTON:
        ((ToggleButtonWidget*)widget)->text1.fixOffset(assets);
        ((ToggleButtonWidget*)widget)->text2.fixOffset(assets);
        break;
    case WIDGET_TYPE_UP_DOWN:
        ((UpDownWidget*)widget)->downButtonText.fixOffset(assets);
        ((UpDownWidget*)widget)->upButtonText.fixOffset(assets);
        break;
    }
}

void fixOffsets(Assets *assets, ListOfAssetsPtr<Value> &values) {
    values.fixOffset(assets);

    for (uint32_t i = 0; i < values.count; i++) {
        auto value = values.item(i);
        fixOffsets(assets, *value);
    }
}

void fixOffsets(Assets *assets, Value &value) {
    if (value.getType() == VALUE_TYPE_STRING) {
        auto assetsPtr = (AssetsPtr<const char> *)&value.uint32Value;
        assetsPtr->fixOffset(assets);
        value.strValue = assetsPtr->ptr();
    } else if (value.getType() == VALUE_TYPE_ARRAY) {
        auto assetsPtr = (AssetsPtr<ArrayValue> *)&value.uint32Value;
        assetsPtr->fixOffset(assets);
        value.arrayValue = assetsPtr->ptr();
        for (uint32_t i = 0; i < value.arrayValue->arraySize; i++) {
            fixOffsets(assets, value.arrayValue->values[i]);
        }
    }
}

} // namespace gui
} // namespace eez
