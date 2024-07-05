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

#pragma once

#include <QDir>
#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	explicit MainWindow(QWidget *parent = nullptr);
	~MainWindow() override;

private slots:
	void on_chooseField_clicked();

	void on_openTasks_clicked();

	void on_runCheckButton_clicked();

	void on_wPcheckBox_stateChanged(int state);

	void on_wPPCheckBox_stateChanged(int state);

	void on_resetPCheckBox_stateChanged(int state);

	void on_showConsoleCheckBox_stateChanged(int state);

	void on_closeOnSuccessOption_stateChanged(int state);

private:
	QDir chooseDirectoryDialog();

	void resetUiOptions(const QHash<QString, QVariant> &options);

	void loadSettings();

	void saveSettings();

	void showNoQrsTsjMessage();

	void showNoFieldsMessage();

	Ui::MainWindow *mUi;
	QDir mTasksDir;
	QDir mFieldsDir;
	QString mTasksPath;
	QDir mStudioDir;
	QString mLocalSettings;

	QHash<QString, QHash<QString, QVariant>> mDirOptions;
};
