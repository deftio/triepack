import org.jetbrains.kotlin.gradle.dsl.JvmTarget

plugins {
    // 1.9.24 matches the compiler scripts/test-jvm.sh uses, so the two paths
    // build with the same Kotlin. 1.9.0 did not know JVM target 21 and failed
    // outright on a JDK 21 machine.
    kotlin("jvm") version "1.9.24"
}

group = "com.deftio"
version = "1.3.2"

repositories {
    mavenCentral()
}

dependencies {
    testImplementation(kotlin("test"))
}

// Pin the bytecode target rather than inheriting whatever JDK happens to be
// installed: otherwise the build depends on the machine it runs on.
kotlin {
    compilerOptions {
        jvmTarget.set(JvmTarget.JVM_17)
    }
}

java {
    sourceCompatibility = JavaVersion.VERSION_17
    targetCompatibility = JavaVersion.VERSION_17
}

tasks.test {
    useJUnitPlatform()
    testLogging {
        events("passed", "skipped", "failed")
        showStandardStreams = false
    }
}
