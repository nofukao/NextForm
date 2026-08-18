<!DOCTYPE html>
<html lang="<?php trw_lang(); ?>">
  <head>
    <?php trw_head_tags(); ?>
    <meta name="viewport" content="width=device-width" />
    <script type="text/javascript" src="<?php trw_theme_url('script/theme.js');?>" ></script>
    <link rel="stylesheet" type="text/css" href="<?php trw_theme_url('style/main.css'); ?>" media="all" /> 
    <noscript>
      <link rel="stylesheet" type="text/css" href="<?php trw_theme_url('style/noscript.css'); ?>" media="all" />
    </noscript>
    <?php if(THEME_IMAGE_ICON !== '') { ?>
    <link rel="shortcut icon" href="<?php trw_echo(THEME_IMAGE_ICON);?>" type="image/x-icon" />
    <link rel="icon" href="<?php trw_echo(THEME_IMAGE_ICON);?>" type="image/x-icon" />
    <?php } ?>
    <title><?php trw_title_head(); ?></title>
  </head>
  <body>
    <header class="title">
      <?php trw_tools('search'); ?>
      <a href="<?php trw_site_url(); ?>" class="title">
        <?php if(THEME_IMAGE_LOGO !== '') { ?>
        <img class="logo" src="<?php trw_echo(THEME_IMAGE_LOGO);?>" alt="site logo" />
        <?php } ?>
        <h1><?php trw_site_name(); ?></h1>
      </a>
    </header>
    <nav class="site_menu">
      <?php trw_tools('site'); ?>
      <?php trw_tools('user'); ?>
    </nav>

    <article class="main">

      <h1><?php trw_title_html(); ?></h1>
      <nav class="page_menu">
        <?php trw_page_menu(); ?>
      </nav>
      <?php trw_message(); ?>

      <?php trw_main_contents(); ?>

      <footer>
        <?php trw_page_tags(); ?>
        <?php trw_page_info(); ?>
      </footer>

    </article>

    <?php if(SIDE_PAGENAME !== '') { ?>
    <article class="side">
      <?php trw_page(SIDE_PAGENAME); ?>
    </article>
    <?php } ?>

    <footer class="signature">
      Powered by 
      <a href="<?php trw_official_url(); ?>" target="_blank">
        <img class="signature" src="resource/image/signature.png"  alt="ToraToraWiki logo"/>
        ToraToraWiki
      </a>
    </footer>
  </body>
</html>
