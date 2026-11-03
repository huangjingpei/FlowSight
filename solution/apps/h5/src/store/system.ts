import { defineStore } from 'pinia';
import { ref } from 'vue';
import { h5Api } from '../api';

export const useSystemStore = defineStore('system', () => {
  const videoPlaybackEnabled = ref(false);
  const loaded = ref(false);

  const fetchFeatures = async () => {
    try {
      const res: any = await h5Api.getSystemFeatures();
      if (res && typeof res.videoPlaybackEnabled === 'boolean') {
        videoPlaybackEnabled.value = res.videoPlaybackEnabled;
      }
      loaded.value = true;
    } catch (e) {
      videoPlaybackEnabled.value = false;
      loaded.value = true;
    }
  };

  return {
    videoPlaybackEnabled,
    loaded,
    fetchFeatures
  };
});
