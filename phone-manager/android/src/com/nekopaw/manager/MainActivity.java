package com.nekopaw.manager;
import android.app.Activity;
import android.app.AlertDialog;
import android.os.Bundle;
import android.content.Intent;
import android.graphics.Color;
import android.net.Uri;
import android.provider.Settings;
import android.view.View;
import android.webkit.*;
import android.widget.*;
import java.io.*;
import java.net.*;
import java.util.concurrent.*;
import org.json.JSONObject;

public final class MainActivity extends Activity {
 private WebView web;private TextView status;private String base,path="/memo";private boolean loadFailed=false;
 private final ExecutorService executor=Executors.newSingleThreadExecutor();private Button test;
 private int dp(int n){return (int)(n*getResources().getDisplayMetrics().density+.5f);}
 private Button button(String title,Runnable action){Button b=new Button(this);b.setText(title);b.setAllCaps(false);b.setTextSize(13);b.setOnClickListener(v->action.run());return b;}
 @Override public void onCreate(Bundle state){
  super.onCreate(state);base=getPreferences(MODE_PRIVATE).getString("device",DeviceAddress.DEFAULT);
  try{base=DeviceAddress.normalize(base);}catch(Exception ignored){base=DeviceAddress.DEFAULT;}
  if(state!=null){String saved=state.getString("path","/memo");for(String p:DeviceAddress.PATHS)if(p.equals(saved))path=p;}
  LinearLayout root=new LinearLayout(this);root.setOrientation(LinearLayout.VERTICAL);root.setBackgroundColor(Color.rgb(237,244,250));
  root.setOnApplyWindowInsetsListener((v,i)->{v.setPadding(i.getSystemWindowInsetLeft(),i.getSystemWindowInsetTop(),i.getSystemWindowInsetRight(),i.getSystemWindowInsetBottom());return i;});
  LinearLayout bar=new LinearLayout(this);bar.setPadding(dp(6),0,dp(6),0);
  bar.addView(button("设备地址",()->addressDialog(false)));test=button("测试连接",()->testConnection());bar.addView(test);
  bar.addView(button("刷新",()->{if(web!=null)web.reload();}));root.addView(bar);
  status=new TextView(this);status.setTextSize(12);status.setPadding(dp(12),dp(3),dp(12),dp(5));root.addView(status);
  HorizontalScrollView scroll=new HorizontalScrollView(this);LinearLayout tabs=new LinearLayout(this);
  String[] names={"宠物","课表","天气","日历","Codex","外卖","备忘录"};
  for(int i=0;i<names.length;i++){final String selected=DeviceAddress.PATHS[i];tabs.addView(button(names[i],()->open(selected)));}
  scroll.addView(tabs);root.addView(scroll);
  web=new WebView(this);web.setBackgroundColor(Color.WHITE);
  WebSettings settings=web.getSettings();settings.setJavaScriptEnabled(true);settings.setDomStorageEnabled(true);settings.setAllowFileAccess(false);settings.setAllowContentAccess(false);settings.setMixedContentMode(WebSettings.MIXED_CONTENT_NEVER_ALLOW);settings.setCacheMode(WebSettings.LOAD_NO_CACHE);
  web.setWebViewClient(new WebViewClient(){
   @Override public void onPageStarted(WebView view,String url,android.graphics.Bitmap icon){loadFailed=false;}
   @Override public boolean shouldOverrideUrlLoading(WebView view,WebResourceRequest request){
    String url=request.getUrl().toString();if(DeviceAddress.sameDevice(base,url))return false;
    if(request.isForMainFrame()&&request.hasGesture()&&("https".equals(request.getUrl().getScheme())||"http".equals(request.getUrl().getScheme()))){try{startActivity(new Intent(Intent.ACTION_VIEW,request.getUrl()));}catch(Exception ignored){}}
    return true;
   }
   @Override public WebResourceResponse shouldInterceptRequest(WebView view,WebResourceRequest request){
    String scheme=request.getUrl().getScheme();if("data".equals(scheme)||DeviceAddress.sameDevice(base,request.getUrl().toString()))return null;
    return new WebResourceResponse("text/plain","UTF-8",403,"Blocked",null,new ByteArrayInputStream(new byte[0]));
   }
   @Override public void onPageFinished(WebView view,String url){if(!loadFailed&&DeviceAddress.sameDevice(base,url))status.setText("设备："+base+" · 请在页面里修改并保存");}
   @Override public void onReceivedError(WebView view,WebResourceRequest request,WebResourceError error){if(request.isForMainFrame()){loadFailed=true;status.setText("未连接设备：检查热点、ESP32地址，再点刷新。可点“测试连接”排查。");}}
   @Override public void onReceivedHttpError(WebView view,WebResourceRequest request,WebResourceResponse response){if(request.isForMainFrame()){loadFailed=true;status.setText("设备返回HTTP "+response.getStatusCode()+"；若404，请上传包含该功能的固件。");}}
  });
  root.addView(web,new LinearLayout.LayoutParams(-1,0,1));setContentView(root);
  if(getPreferences(MODE_PRIVATE).getBoolean("configured",false))open(path);else addressDialog(true);
 }
 private void open(String value){path=value;status.setText("正在打开 "+base+path);web.loadUrl(base+path);}
 private void addressDialog(boolean first){
  EditText input=new EditText(this);input.setSingleLine(true);input.setText(base);input.setSelectAllOnFocus(true);input.setInputType(android.text.InputType.TYPE_CLASS_TEXT|android.text.InputType.TYPE_TEXT_VARIATION_URI);
  LinearLayout box=new LinearLayout(this);box.setOrientation(1);box.setPadding(dp(20),dp(8),dp(20),dp(8));
  TextView help=new TextView(this);help.setText("手机开热点，让ESP32连这个热点；或让手机和ESP32连接同一Wi-Fi。\n填写ESP32地址，不是手机热点网关。IP变化时可在这里修改。");box.addView(help);box.addView(input);
  AlertDialog dialog=new AlertDialog.Builder(this).setTitle("连接墨水屏").setView(box).setPositiveButton("保存并打开",null).setNeutralButton("Wi-Fi设置",(d,w)->{try{startActivity(new Intent(Settings.ACTION_WIFI_SETTINGS));}catch(Exception ignored){}if(first)status.setText("回到App后点“设备地址”继续设置。");}).setNegativeButton("取消",(d,w)->{if(first)status.setText("请点“设备地址”设置后再连接。");}).create();
  dialog.setOnShowListener(d->dialog.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener(v->{try{String value=DeviceAddress.normalize(input.getText().toString());if(!value.equals(base))web.clearHistory();base=value;getPreferences(MODE_PRIVATE).edit().putString("device",base).putBoolean("configured",true).apply();dialog.dismiss();open(path);}catch(Exception e){input.setError(e.getMessage());}}));dialog.show();
 }
 private void testConnection(){
  test.setEnabled(false);status.setText("正在测试设备…");final String target=base;
  executor.execute(()->{String message;HttpURLConnection connection=null;try{
   connection=(HttpURLConnection)new URL(target+"/api/page").openConnection();connection.setConnectTimeout(6000);connection.setReadTimeout(6000);connection.setInstanceFollowRedirects(false);
   int code=connection.getResponseCode();if(code!=200)throw new IOException(code==404?"设备固件没有统一切页接口，请更新固件":"设备返回HTTP "+code);
   ByteArrayOutputStream bytes=new ByteArrayOutputStream();try(InputStream in=connection.getInputStream()){byte[] buffer=new byte[1024];int n;while((n=in.read(buffer))!=-1){bytes.write(buffer,0,n);if(bytes.size()>32768)throw new IOException("设备响应异常");}}
   JSONObject data=new JSONObject(bytes.toString("UTF-8"));if(!"nekopaw-pages-v2".equals(data.optString("service")))throw new IOException("该地址不是当前墨水屏管理接口");
   message="已连接ESP32，共"+data.optInt("pageCount")+"页；点击上方功能进入编辑。";
  }catch(Exception e){message="连接失败："+(e.getMessage()==null?e.getClass().getSimpleName():e.getMessage())+"。请核对热点和IP地址。";}finally{if(connection!=null)connection.disconnect();}
   final String result=message;runOnUiThread(()->{if(!isFinishing()){test.setEnabled(true);if(target.equals(base))status.setText(result);}});
  });
 }
 @Override public void onBackPressed(){if(web!=null&&web.canGoBack())web.goBack();else super.onBackPressed();}
 @Override protected void onSaveInstanceState(Bundle out){out.putString("path",path);super.onSaveInstanceState(out);}
 @Override protected void onDestroy(){executor.shutdownNow();if(web!=null){web.stopLoading();web.destroy();web=null;}super.onDestroy();}
}
