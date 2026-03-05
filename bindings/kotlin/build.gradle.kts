plugins {
    kotlin("jvm") version "1.9.0"
}

group = "com.deftio"
version = "0.1.0"

repositories {
    mavenCentral()
}

dependencies {
    testImplementation(kotlin("test"))
}

tasks.test {
    useJUnitPlatform()
}
