/* Copyright 2021 CyberTech Labs Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License. */

#include <QString>
#include <QVariant>

#pragma once

const QString closeSuccessOption = "closeOnSuccess";
const QString backgroundOption = "background";
const QString consoleOption = "showConsole";

const QString xmlFieldsDir = "fieldsDir";
const QString patchField = "patchField";
const QString patchWorld = "patchWorld";
const QString patchWP = "patchWroldAndPosition";
const QString resetRP = "resetRobotPosition";

const QHash<QString, QVariant> defaultOptions{{closeSuccessOption, true}, {backgroundOption, false},
					      {consoleOption, false},	  {xmlFieldsDir, ""},
					      {patchField, true},	  {patchWP, false}};
