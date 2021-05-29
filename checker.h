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

	void revieweTasks(const QFileInfoList &qrsInfos, const QFileInfoList &fieldsInfos, const QHash<QString
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
		QString error;
		QString time;
	};

private:
	static bool compareReportsByTask(const TaskReport &first, const TaskReport &second)
	{
		return first.task < second.task;
	}

	static void reduceFunction(QHash<QString, QList<TaskReport>> &result, const QList<TaskReport> &intermediate);

	static QList<TaskReport> checkTask(const Task *task);

	static QString startProcess(const QString &program, const QStringList &options);

	void createHtmlReport(QHash<QString, QList<TaskReport>> &result);

	const QStringList generateRunnerOptions(const QHash<QString, QVariant> &options);

	const QStringList generatePathcerOptions(const QHash<QString, QVariant> &options);

	static bool isErrorMessage(const QString &message);

	const QString &mTasksPath;
};
