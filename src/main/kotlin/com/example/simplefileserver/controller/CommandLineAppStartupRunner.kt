package com.example.simplefileserver.controller

import org.springframework.beans.factory.annotation.Autowired
import org.springframework.boot.ApplicationArguments
import org.springframework.boot.ApplicationRunner
import org.springframework.stereotype.Component
import java.io.File

@Component
class CommandLineAppStartupRunner @Autowired constructor(
    private val appConfig: AppConfig
) : ApplicationRunner {
    /**
     * From ChatGpt-4o
     */
    override fun run(args: ApplicationArguments) {
        val nonOptionDirectoryArg = args.nonOptionArgs?.firstOrNull() // e.g. java -jar fileserver.jar D:/
        if (!nonOptionDirectoryArg.isNullOrEmpty()) {
            appConfig.directory = nonOptionDirectoryArg
        }

        val directoryArg = args.getOptionValues("directory")?.firstOrNull() // 显式指定目录
        if (!directoryArg.isNullOrEmpty()) {
            appConfig.directory = directoryArg
        }

        val inlineDisplayArg = args.getOptionValues("inline-display")?.firstOrNull()
        if (!inlineDisplayArg.isNullOrEmpty()) {
            appConfig.inlineDisplay = inlineDisplayArg.toBoolean()
        }

        // 如果未指定目录，指定当前运行的目录
        if (appConfig.directory.isEmpty()) {
            appConfig.directory = File("").absolutePath
        }

    }

}