package com.droiddeck.launcher.gpu

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Test

class LinuxVulkanDriverIdTest {
    @Test fun importsNeverGetTheVortekId() {
        assertNotEquals(LinuxVulkanDriverManager.VORTEK_ID, LinuxVulkanDriverManager.uniqueId("vortek") { false })
        assertEquals("vortek-imported", LinuxVulkanDriverManager.uniqueId(LinuxVulkanDriverManager.sanitizeId("vortek")) { false })
        assertEquals("vortek-imported-2", LinuxVulkanDriverManager.uniqueId("vortek") { it == "vortek-imported" })
    }

    @Test fun otherIdsUnchanged() {
        assertEquals("turnip", LinuxVulkanDriverManager.uniqueId("turnip") { false })
        assertEquals("turnip-2", LinuxVulkanDriverManager.uniqueId("turnip") { it == "turnip" })
        assertEquals("vortek2", LinuxVulkanDriverManager.uniqueId("vortek2") { false })
    }

    @Test fun libraryNameIsPerDriver() {
        assertEquals("libvulkan_vortek.so", LinuxVulkanDriverManager.libraryName(LinuxVulkanDriverManager.VORTEK_ID))
        assertEquals(LinuxVulkanDriverManager.LIB_NAME, LinuxVulkanDriverManager.libraryName("turnip"))
    }

    @Test fun icdApiVersionFromMeta() {
        assertEquals("1.3.289", LinuxVulkanDriverManager.icdApiVersion("1.3.289"))
        assertEquals("1.1.274", LinuxVulkanDriverManager.icdApiVersion(""))
        assertEquals("1.1.274", LinuxVulkanDriverManager.icdApiVersion(null))
        assertEquals("1.1.274", LinuxVulkanDriverManager.icdApiVersion("2.0.0"))
        assertEquals("1.1.274", LinuxVulkanDriverManager.icdApiVersion("1.3"))
    }
}
