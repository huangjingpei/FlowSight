import { defineStore } from 'pinia';
import { ref } from 'vue';

export type ThemeMode = 'dark' | 'light';
export type FontSizeScale = 'standard' | 'medium' | 'large';

export const useThemeStore = defineStore('theme', () => {
  const currentTheme = ref<ThemeMode>(
    (localStorage.getItem('flowsight_theme') as ThemeMode) || 'dark'
  );
  const currentFontScale = ref<FontSizeScale>(
    (localStorage.getItem('flowsight_font_scale') as FontSizeScale) || 'standard'
  );

  const applyThemeToDom = (theme: ThemeMode) => {
    const root = document.documentElement;
    if (theme === 'light') {
      root.classList.add('theme-light');
      root.classList.remove('theme-dark');
      root.setAttribute('data-theme', 'light');
    } else {
      root.classList.add('theme-dark');
      root.classList.remove('theme-light');
      root.setAttribute('data-theme', 'dark');
    }
  };

  const applyFontScaleToDom = (scale: FontSizeScale) => {
    const root = document.documentElement;
    root.classList.remove('font-scale-standard', 'font-scale-medium', 'font-scale-large');
    root.classList.add(`font-scale-${scale}`);
    root.setAttribute('data-font-scale', scale);
  };

  const setTheme = (theme: ThemeMode) => {
    currentTheme.value = theme;
    try {
      localStorage.setItem('flowsight_theme', theme);
    } catch (e) {}
    applyThemeToDom(theme);
  };

  const setFontScale = (scale: FontSizeScale) => {
    currentFontScale.value = scale;
    try {
      localStorage.setItem('flowsight_font_scale', scale);
    } catch (e) {}
    applyFontScaleToDom(scale);
  };

  const init = () => {
    applyThemeToDom(currentTheme.value);
    applyFontScaleToDom(currentFontScale.value);
  };

  return {
    currentTheme,
    currentFontScale,
    setTheme,
    setFontScale,
    init
  };
});
