/*
 * EEZ DIB SMX46
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

#include "./dib-smx46.h"

namespace eez {
namespace dib_smx46 {

static ModuleInfo g_thisModuleInfo(MODULE_TYPE_DIB_SMX46, MODULE_CATEGORY_OTHER, "smx46", MODULE_REVISION_R1B2);
ModuleInfo *g_moduleInfo = &g_thisModuleInfo;

} // namespace dib_smx46
} // namespace eez