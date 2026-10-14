package com.droiddeck.launcher.input

import org.junit.Assert.*
import org.junit.Test
import java.nio.file.Files

class PadIdentityFileTest {
    @Test fun identityFileIsReplacedAtomically() {
        val dir = Files.createTempDirectory("ident").toFile()
        val target = java.io.File(dir, "identity1")
        assertTrue(PadProfiles.writeIdentityFile(target, "ps5"))
        assertEquals("ps5\n", target.readText())
        assertTrue(PadProfiles.writeIdentityFile(target, "xbox360"))
        assertEquals("xbox360\n", target.readText())
        assertFalse(java.io.File(dir, "identity1.tmp").exists())
        dir.deleteRecursively()
    }
}
