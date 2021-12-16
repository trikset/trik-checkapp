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

#include "checker.h"

#include <QProcess>
#include <QtConcurrent>
#include <QProgressDialog>
#include <QTime>

#include "optionsAliases.h"
#include "htmlTemplates.h"

const int BACKGROUND_TIMELIMIT = 20 * 1000;
const int MAX_VISIBLE_THREADS = 2;

Checker::Checker(const QString &tasksPath)
	: mTasksPath(tasksPath)
{
}

void Checker::revieweTasks(const QFileInfoList &qrsInfos, const QFileInfoList &fieldsInfos, const QHash<QString, QVariant> &options)
{
	auto patcherOptions = generatePathcerOptions(options);
	auto runnerOptions = generateRunnerOptions(options);

	QList<Task *> tasksList;
	for (auto &&qrs : qrsInfos) {
		tasksList += new Task({qrs, fieldsInfos, patcherOptions, runnerOptions});
	}

	QProgressDialog dialog;
	dialog.setCancelButtonText(tr("Cancel"));
	dialog.setWindowTitle("TRIK CheckApp");
	dialog.setWindowFlags(dialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);
	dialog.setLabelText(tr("A check is performed..."));

	QFutureWatcher<QHash<QString, QList<TaskReport>>> watcher;
	connect(&dialog,  &QProgressDialog::canceled, &watcher
			, &QFutureWatcher<QHash<QString, QList<TaskReport>>>::cancel);
	connect(&watcher, &QFutureWatcher<QHash<QString, QList<TaskReport>>>::progressRangeChanged
			, &dialog, &QProgressDialog::setRange);
	connect(&watcher, &QFutureWatcher<QHash<QString, QList<TaskReport>>>::progressValueChanged
			, &dialog, &QProgressDialog::setValue);
	connect(&watcher, &QFutureWatcher<QHash<QString, QList<TaskReport>>>::finished,
			this, [this, &dialog, &watcher](){
		dialog.setLabelText(tr("Creating a report"));
		if (!watcher.isCanceled()) {
			auto result = watcher.result();
			this->createHtmlReport(result);
		}
		dialog.reset();
	});

	if (!options[backgroundOption].toBool()) {
		QThreadPool::globalInstance()->setMaxThreadCount(MAX_VISIBLE_THREADS);
	}

	auto futureTasks = QtConcurrent::mappedReduced(tasksList, checkTask, reduceFunction);
	watcher.setFuture(futureTasks);

	dialog.exec();
	if (dialog.wasCanceled()) {
		watcher.waitForFinished();
	}

	for(auto &&t : tasksList) {
		delete t;
	}
}

QList<Checker::TaskReport> Checker::checkTask(const Checker::Task *t)
{
	QList<TaskReport> result;
	QString ext = "";
	if (QOperatingSystemVersion::currentType() == QOperatingSystemVersion::Windows) {
		ext = ".exe";
	}

	for (auto &&f : t->fieldsInfos) {
		QDir(t->qrs.absoluteDir().absolutePath()).mkdir("tmp");
		const QString patchedQrsName = t->qrs.absoluteDir().absolutePath() + "/tmp/" + t->qrs.fileName();
		QFile(t->qrs.absoluteFilePath()).copy(patchedQrsName);
		QFile patchedQrs(patchedQrsName);

		startProcess("patcher" + ext, QStringList(patchedQrs.fileName()) + t->patcherOptions + QStringList(f.absoluteFilePath()));

		TaskReport report;
		report.name = t->qrs.fileName();
		report.task = f.fileName();
		report.error = startProcess("2D-model" + ext, QStringList(patchedQrs.fileName()) + t->runnerOptions);
		report.time = "-";

		if (!isErrorMessage(report.error)) {
			int start = report.error.indexOf(tr("in")) + 3;
			int end = report.error.indexOf(tr("sec!")) - 1;
			if (start - 3 != -1 && end + 1 != -1) {
				report.time = report.error.mid(start, end - start);
			}
		}

		result.append(report);
	}
	QDir(t->qrs.absoluteDir().absolutePath() + "/tmp/").removeRecursively();

	return result;
}

void Checker::reduceFunction(QHash<QString, QList<TaskReport>> &result, const QList<TaskReport> &intermediate)
{
	for (auto i : intermediate) {
		result[i.name].append(i);
	}
}

QString Checker::startProcess(const QString &program, const QStringList &options)
{
	QProcess proccess;
	proccess.start(program, options);
	if (!proccess.waitForStarted()) {
		return "Error: not started";
	}

	if (options.contains("-b") && !proccess.waitForFinished(BACKGROUND_TIMELIMIT)) {
		return "Error: not finished";
	}
	else {
		proccess.waitForFinished(-1);
	}

	return proccess.readAllStandardError();
}

void Checker::createHtmlReport(QHash<QString, QList<TaskReport>> &result)
{
	auto qrsNames = result.keys();
	auto numberOfCorrect = new int[qrsNames.length()] {0};
	std::sort(qrsNames.begin(), qrsNames.end());

	int i = 0;
	for (auto &&key : qrsNames) {
		std::sort(result[key].begin(), result[key].end(), compareReportsByTask);
		foreach (auto r, result[key]) {
			numberOfCorrect[i] += isErrorMessage(r.error) ? 0 : 1;
		}
		i++;
	}

	QFile reportFile(mTasksPath + QDir::separator() + "report.html");
	QFile htmlBegin(":/report_begin.html");
	QFile htmlEnd(":/report_end.html");

	QString body = reportHeader.arg(mTasksPath.section(QDir::separator(), -1)).arg(QDateTime::currentDateTime().toString("hh:mm dd.MM.yyyy"));

	i = 0;
	for (auto &&key : qrsNames) {
		auto studentResults = result[key];

		QString color = yellowCssClass;
		if (numberOfCorrect[i] == studentResults.length()) {
			color = greenCssClass;
		} else if (numberOfCorrect[i] == 0) {
			color = blackCssClass;
		}

		int counter = 0;
		QString name;
		for (auto &&r : studentResults) {
			name = "";
			if (counter == 0) {
				name = r.name;
			} else if (counter == 1) {
				color = "";
				name = QString(tr("Total %1 of %2")).arg(numberOfCorrect[i]).arg(result[key].length());
			}
			QString status = isErrorMessage(r.error) ? tr("Error") : tr("Complete");
			body += taskReport.arg(color).arg(name).arg(r.task).arg(status).arg(r.time);

			counter++;
		}
		i++;
	}

	htmlBegin.open(QFile::ReadOnly);
	QString report = htmlBegin.readAll();
	htmlBegin.close();

	report += body;

	htmlEnd.open(QFile::ReadOnly);
	report += htmlEnd.readAll();
	htmlEnd.close();

	std::string html = report.toStdString();
	const auto raw = html.c_str();
	reportFile.open(QIODevice::WriteOnly | QIODevice::Truncate);
	reportFile.write(raw);
	reportFile.close();

	delete[] numberOfCorrect;
}

const QStringList Checker::generateRunnerOptions(const QHash<QString, QVariant> &options)
{
	QStringList result;
	if (options[closeSuccessOption].toBool())
		result << "--close-on-succes";

	if (options[backgroundOption].toBool())
		result << "-b";

	if (options[consoleOption].toBool())
		result << "-c";

	return result;
}

const QStringList Checker::generatePathcerOptions(const QHash<QString, QVariant> &options)
{
	QStringList result;

	if (options[resetRP].toBool()) {
		result << "--rrp";
	}

	if (options[patchField].toBool()) {
		result << "-f";
	}
	else {
		if (options[patchWP].toBool()) {
			result << "--wp";
		}
		else {
			result << "-w";
		}
	}

	return result;
}

bool Checker::isErrorMessage(const QString &message)
{
	return message.indexOf(tr("Error")) != -1 or message.indexOf("Error") != -1;
}
