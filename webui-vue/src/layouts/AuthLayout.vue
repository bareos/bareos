<template>
  <q-layout view="hHh lpR fFf" style="background: linear-gradient(#ffffff, #0075be); min-height: 100vh">
    <q-page-container>
      <InsecureConnectionBanner />
      <router-view />
    </q-page-container>
  </q-layout>
</template>

<script setup>
import InsecureConnectionBanner from '../components/InsecureConnectionBanner.vue'
</script>
