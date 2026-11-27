<template>
  <q-layout view="hHh lpR fFf">
    <q-page-container>
      <InsecureConnectionBanner />
      <router-view />
    </q-page-container>
  </q-layout>
</template>

<script setup>
import InsecureConnectionBanner from '../components/InsecureConnectionBanner.vue'
</script>
