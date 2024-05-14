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

#include <QFileInfo>
#include <QString>

class Checker : public QObject
{
	Q_OBJECT

public:
	Checker(const QString &tasksPath);

	void reviewTasks(const QFileInfoList &qrsInfos, const QFileInfoList &fieldsInfos, const QHash<QString
			  , QVariant> &options);
	struct Task {
		QFileInfo qrs;
		const QFileInfoList &fieldsInfos;
		const QStringList &patcherOptions;
		const QStringList &runnerOptions;
	};

	struct TaskReport {
		QString name;
		QString task;
		QString time;
		QString message;
		QString level;

		bool operator <(const TaskReport& other) const { return task < other.task; }
	};

private:
	typedef QList<TaskReport> task_results_t;

	static void reduceFunction(QHash<QString, task_results_t> &result, const task_results_t &intermediate);

	static task_results_t checkTask(const Task *task);

	static QString executeProcess(const QString &program, const QStringList &options);

	static QPair<QString, QString> handleJsonReport(const QString &filename);

	const QString createHtmlReport(const QHash<QString, QList<TaskReport> > &result);

	const QStringList generateRunnerOptions(const QHash<QString, QVariant> &options);

	const QStringList generatePatcherOptions(const QHash<QString, QVariant> &options);

	static bool isErrorMessage(const QString &message);

	static bool isErrorReport(const TaskReport &message);

	static QString getErrorMessage(const QString &message);

	const QString &mTasksPath;

	void createTasksEnvironment();

	void removeTasksEnvironment();
};
