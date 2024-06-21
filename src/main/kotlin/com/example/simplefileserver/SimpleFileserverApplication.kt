package com.example.simplefileserver

import com.example.simplefileserver.controller.AppConfig
import org.springframework.boot.autoconfigure.SpringBootApplication
import org.springframework.boot.context.properties.EnableConfigurationProperties
import org.springframework.boot.runApplication

@SpringBootApplication
@EnableConfigurationProperties(AppConfig::class)
class SimpleFileserverApplication

fun main(args: Array<String>) {
	runApplication<SimpleFileserverApplication>(*args)
}
