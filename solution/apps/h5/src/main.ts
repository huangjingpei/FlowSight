import { createApp } from 'vue';
import { createPinia } from 'pinia';
import App from './App.vue';
import router from './router';

// 引入 Vant 样式
import 'vant/lib/index.css';
import Vant from 'vant';

const app = createApp(App);
app.use(createPinia());
app.use(router);
app.use(Vant);

app.mount('#app');
