import { createApp } from 'vue';
import { createPinia } from 'pinia';
import App from './App.vue';
import router from './router';

// 引入 Vant 样式
import 'vant/lib/index.css';
import Vant from 'vant';

const app = createApp(App);
const pinia = createPinia();
app.use(pinia);
app.use(router);
app.use(Vant);

// 初始化并恢复全局显示主题与字号缩放无障碍配置
import { useThemeStore } from './store/theme';
const themeStore = useThemeStore();
themeStore.init();

app.mount('#app');
