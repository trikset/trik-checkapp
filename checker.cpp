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
#include <QtConcurrent/QtConcurrent>
#include <QProgressDialog>
#include <QTime>
#include <QDesktopServices>
#include "optionsAliases.h"
#include "htmlTemplates.h"

const int BACKGROUND_TIMELIMIT = 20 * 1000;
const int MAX_VISIBLE_THREADS = 2;
const QString TEMP_POSTFIX = "tmp_patched_qrs";

#ifdef Q_OS_LINUX
void gnomeEnvironmentHandler(QProcess &process) {
	QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
	QString xdgCurrentDesktop = env.value("XDG_CURRENT_DESKTOP");
	if (xdgCurrentDesktop.contains("GNOME")) {
		QStringList environment;
		environment << "QT_QPA_PLATFORM=xcb";
		environment << "DISPLAY=:0";
		process.setEnvironment(environment);
	}
}
#endif

Checker::Checker(const QString &tasksPath) : mTasksPath(tasksPath) {}

void Checker::reviewTasks(const QFileInfoList &qrsInfos, const QFileInfoList &fieldsInfos,
			  const QHash<QString, QVariant> &options) {
	auto patcherOptions = generatePatcherOptions(options);
	auto runnerOptions = generateRunnerOptions(options);

	QProgressDialog dialog;
	dialog.setCancelButtonText(tr("Cancel"));
	dialog.setWindowTitle("TRIK CheckApp");
	dialog.setWindowFlags(dialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);
	dialog.setLabelText(tr("A check is performed..."));

	QFutureWatcher<QHash<QString, QList<TaskReport>>> watcher;
	connect(&dialog, &QProgressDialog::canceled, &watcher,
		&QFutureWatcher<QHash<QString, QList<TaskReport>>>::cancel);
	connect(&watcher, &QFutureWatcher<QHash<QString, QList<TaskReport>>>::progressRangeChanged, &dialog,
		&QProgressDialog::setRange);
	connect(&watcher, &QFutureWatcher<QHash<QString, QList<TaskReport>>>::progressValueChanged, &dialog,
		&QProgressDialog::setValue);
	connect(&watcher, &QFutureWatcher<QHash<QString, QList<TaskReport>>>::finished, this,
		[this, &dialog, &watcher]() {
			dialog.setLabelText(tr("Creating a report"));
			if (!watcher.isCanceled()) {
				auto r = watcher.result();
				auto keys = r.keys();
				for (auto &x : keys) {
					std::sort(r[x].begin(), r[x].end());
				}

				auto htmlReportName = createHtmlReport(r);
				if (!QFile::exists(htmlReportName)) {
					qDebug() << "Error: Report file not found: " << htmlReportName;
				} else if (!QDesktopServices::openUrl(QUrl::fromLocalFile(htmlReportName))) {
					qDebug() << "Error: Couldn't open url for report file: " << htmlReportName;
				}
			}
			dialog.reset();
		});

	if (!options[backgroundOption].toBool()) {
		QThreadPool::globalInstance()->setMaxThreadCount(MAX_VISIBLE_THREADS);
	}

	QList<Task *> tasksList;
	for (auto &&qrs : qrsInfos) {
		tasksList += new Task({qrs, fieldsInfos, patcherOptions, runnerOptions});
	}
	createTasksEnvironment();
	auto futureTasks = QtConcurrent::mappedReduced(tasksList, checkTask, reduceFunction);
	watcher.setFuture(futureTasks);

	dialog.exec();
	if (dialog.wasCanceled()) {
		watcher.waitForFinished();
	}

	removeTasksEnvironment();
	qDeleteAll(tasksList);
}

void Checker::createTasksEnvironment() { QDir(mTasksPath).mkdir(TEMP_POSTFIX); }

void Checker::removeTasksEnvironment() {
	const QString tmpDirPath = mTasksPath + QDir::separator() + TEMP_POSTFIX;
	QDir(tmpDirPath).removeRecursively();
}

QPair<QString, QString> Checker::handleJsonReport(const QString &filename) {

	QFile file(filename);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		qDebug() << "Error: The report file could not be opened:" << filename;
		return {"", "error"};
	}

	const QString jsonString = file.readAll();
	file.close();

	if (!QFile::remove(filename)) {
		qDebug() << "Error: Couldn't delete the report file: " << filename;
	}

	QJsonParseError parseError{};
	const QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonString.toUtf8(), &parseError);

	if (jsonDoc.isNull() || jsonDoc.isEmpty()) {
		qDebug() << "Error: The report file should not be empty " << parseError.errorString();
		return {"", "error"};
	}

	auto jsonObj = jsonDoc.array().at(0).toObject();
	if (!jsonObj.contains("level") || !jsonObj.contains("message")) {
		qDebug() << "Error: The report file should not be empty " << parseError.errorString();
		return {"", "error"};
	}

	const QString level = jsonObj["level"].toString();
	const QString message = jsonObj["message"].toString();

	return {message, level};
}

Checker::task_results_t Checker::checkTask(const Checker::Task *t) {
	const QString ext = QOperatingSystemVersion::currentType() == QOperatingSystemVersion::Windows ? ".exe" : "";
	const QString tasksPath = t->qrs.absoluteDir().absolutePath();
	const QString tmpDirPath = tasksPath + QDir::separator() + TEMP_POSTFIX;

	task_results_t result;

	static const QRegularExpression pattern(tr("in") + R"( (\d+(\.\d+)?) )" + tr("sec"));

	for (auto &&f : t->fieldsInfos) {
		const QString patchedQrsName = tmpDirPath + QDir::separator() + t->qrs.fileName();
		QFile::copy(t->qrs.absoluteFilePath(), patchedQrsName);
		QFile patchedQrs(patchedQrsName);

		TaskReport report;
		report.name = t->qrs.fileName();
		report.task = f.fileName();
		report.time = "-";

		auto execDir = QCoreApplication::applicationDirPath();
		report.message =
		    executeProcess(execDir + QDir::separator() + "patcher" + ext,
				   QStringList(patchedQrs.fileName()) + t->patcherOptions << f.absoluteFilePath());

		if (isErrorMessage(report.message)) {
			qDebug() << "Error: Failed to patch:" << report.message;
			report.level = "error";
			result.append(report);
			continue;
		}

		const QString reportFileName = tmpDirPath + QDir::separator() + report.name + report.task;
		auto additional_options = QStringList("-r") << reportFileName;

		auto message =
		    executeProcess(execDir + QDir::separator() + "2D-model" + ext,
				   QStringList(patchedQrs.fileName()) + t->runnerOptions + additional_options);

		auto twoModelResult = handleJsonReport(reportFileName);
		report.level = twoModelResult.second;

		if (twoModelResult.first != "") {
			report.message = twoModelResult.first;

			if (twoModelResult.second == "error") {
				qDebug() << "Error: Failed to run 2D-model:" << report.message;
				result.append(report);
				continue;
			}
		}

		else if (twoModelResult.second == "error") {
			report.message = getErrorMessage(message);
			qDebug() << "Error: Failed to run 2D-model:" << message;
			result.append(report);
			continue;
		}

		auto match = pattern.match(report.message);

		if (match.hasMatch()) {
			report.time = match.captured(1) + " " + tr("sec");
		}

		result.append(report);
	}

	return result;
}

void Checker::reduceFunction(QHash<QString, Checker::task_results_t> &result,
			     const Checker::task_results_t &intermediate) {
	for (auto &i : intermediate) {
		result[i.name].append(i);
	}
}

QString Checker::executeProcess(const QString &program, const QStringList &options) {
	QProcess process;

#ifdef Q_OS_LINUX
	gnomeEnvironmentHandler(process);
#endif

	QEventLoop l;

	connect(&process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), &l,
		[&](int exitCode, QProcess::ExitStatus exitStatus) {
			Q_UNUSED(exitCode)
			if (exitStatus == QProcess::ExitStatus::CrashExit) {
				l.exit(-1);
			} else {
				l.exit();
			}
		});

	connect(&process, &QProcess::errorOccurred, &l, [&](QProcess::ProcessError processError) {
		qDebug() << "ERROR" << processError << program << options;
		l.exit(-1);
	});

	if (options.contains("-b")) {
		QTimer::singleShot(BACKGROUND_TIMELIMIT, Qt::TimerType::CoarseTimer, &l, [&]() {
			qDebug() << "ERROR TIMEOUT" << program << options;
			l.exit(-2);
		});
	}

	process.start(program, options);
	auto rc = l.exec();

	if (rc == -1) {
		return "Error: Application proccess crashed. Please, check manually";
	}

	return process.readAllStandardError();
}

QString Checker::createHtmlReport(const QHash<QString, QList<TaskReport>> &result) {
	auto qrsNames = result.keys();
	std::sort(qrsNames.begin(), qrsNames.end());

	const QDateTime dateTime = QDateTime::currentDateTime();
	QString body =
	    reportHeader.arg(mTasksPath.section(QDir::separator(), -1), dateTime.toString("hh:mm:ss dd.MM.yyyy"));

	int i = 0;
	for (auto &&key : qrsNames) {
		int numberOfCorrect = 0;
		auto studentResults = result[key];

		for (auto &&r : studentResults) {
			numberOfCorrect += !isErrorReport(r);
		}

		QString color = yellowCssClass;
		if (numberOfCorrect == studentResults.length()) {
			color = greenCssClass;
		} else if (numberOfCorrect == 0) {
			color = blackCssClass;
		}

		QString name = QString(tr("Total %1 of %2")).arg(numberOfCorrect).arg(studentResults.length());
		body += taskReport.arg(color, key, tr("All"), name, "-");

		for (auto &&r : studentResults) {
			const QString errorMessage = isErrorReport(r) ? r.message : tr("Complete");
			body += taskReport.arg("", "", r.task, errorMessage, r.time);
		}

		i++;
	}

	QFile htmlBegin(":/report_begin.html");
	htmlBegin.open(QFile::ReadOnly);
	QString report = htmlBegin.readAll();
	htmlBegin.close();

	report += body;

	QFile htmlEnd(":/report_end.html");
	htmlEnd.open(QFile::ReadOnly);
	report += htmlEnd.readAll();
	htmlEnd.close();

	const QString reportFileName = QString("report_%1.html").arg(dateTime.toString("dd_MM_yyyy_hh_mm_ss"));
	QFileInfo reportFileInfo(mTasksPath + QDir::separator() + reportFileName);
	QFile reportFile(reportFileInfo.absoluteFilePath());
	if (!reportFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		qDebug() << "Error: Failed to open" << reportFileInfo.absoluteFilePath() << "with"
			 << reportFile.errorString();
	}
	if (reportFile.write(report.toUtf8()) < 0) {
		qDebug() << "Error: Failed to write" << reportFileInfo.absoluteFilePath() << "with"
			 << reportFile.errorString();
	}
	reportFile.close();

	return reportFileInfo.absoluteFilePath();
}

QStringList Checker::generateRunnerOptions(const QHash<QString, QVariant> &options) {
	QStringList result;
	if (options[closeSuccessOption].toBool()) {
		result << "--close-on-success";
	}

	if (options[backgroundOption].toBool()) {
		result << "-b";
	}

	if (options[consoleOption].toBool()) {
		result << "-c";
	}

	return result;
}

QStringList Checker::generatePatcherOptions(const QHash<QString, QVariant> &options) {
	QStringList result;
	if (options[resetRP].toBool()) {
		result << "--rrp";
	}

	if (options[patchField].toBool()) {
		result << "-f";
	} else {
		if (options[patchWP].toBool()) {
			result << "--wp";
		} else {
			result << "-w";
		}
	}

	return result;
}

bool Checker::isErrorMessage(const QString &message) {
	return message.indexOf(tr("Error")) != -1 or message.indexOf("Error") != -1;
}

bool Checker::isErrorReport(const TaskReport &report) { return report.level == "error"; }

QString Checker::getErrorMessage(const QString &message) {
	auto messageLastIndex = message.lastIndexOf(tr("Error"));
	auto endErrorIndex = message.indexOf(QChar::LineFeed, messageLastIndex);

	if (messageLastIndex != -1 and endErrorIndex == -1) {
		return message.mid(messageLastIndex);
	}
	return message.mid(messageLastIndex, endErrorIndex - messageLastIndex + 1);
}
