package com.example.simplefileserver

import com.example.simplefileserver.controller.AppConfig
import org.springframework.boot.autoconfigure.SpringBootApplication
import org.springframework.boot.context.properties.EnableConfigurationProperties
import org.springframework.boot.runApplication
import org.springframework.context.ApplicationContext
import java.net.InetAddress

@SpringBootApplication
@EnableConfigurationProperties(AppConfig::class)
class SimpleFileserverApplication

fun main(args: Array<String>) {
	val context = runApplication<SimpleFileserverApplication>(*args)
	printServerIpAddresses(context)
}

fun printServerIpAddresses(context: ApplicationContext) {
	val localHost = InetAddress.getLocalHost()
	val port = context.environment.getProperty("server.port") ?: "8080"
	println("📁 File server running at: ")
	println("   http://127.0.0.1:${port}/")
	println("   http://${localHost.hostAddress}:${port}/")
}
