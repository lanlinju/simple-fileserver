package com.example.simplefileserver.controller

import org.springframework.boot.context.properties.ConfigurationProperties
import org.springframework.stereotype.Component

@Component
@ConfigurationProperties(prefix = "app")
class AppConfig {
    var directory: String = "" // 要显示的目录
    var inlineDisplay: Boolean = true // 是否播放视频，if是false，直接下载
    lateinit var environment: String // 指定开发环境 dev prod default
}