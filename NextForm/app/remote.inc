<?php
/*
 * 利用者が入れた URL から取ってくる。他の NextForm のページのクローン
 * (option/clone.inc) が使う。
 *
 * URL を決めるのは利用者なので、そのまま取りに行くと、wiki のサーバーから
 * 社内や自宅 LAN の機器 (ルーターの管理画面など) にアクセスさせられる。
 * 取れた中身がそのまま見えなくても、応答の違いから「そこに何があるか」を
 * 探れてしまう。そこで既定では、インターネット上の公開のアドレス
 * (FILTER_FLAG_GLOBAL_RANGE) にだけ接続する。プライベートなネットワークの
 * ホストは、呼び出し側が渡す許可の一覧 ($allowed_hosts) にあるときだけ許す。
 *
 * 名前を引いて確かめた IP にそのまま接続する。名前で接続し直すと、確かめた
 * あとで名前の向き先を内側に変える攻撃 (DNS rebinding) を防げない。
 * https では、証明書の確認と SNI に元のホスト名を使う (ssl の peer_name)。
 * 転送 (Location) は自分でたどり、行き先ごとに同じ確認をする。
 *
 * 待ち時間の上限は EXTERNAL_FETCH_TIMEOUT。受け取る大きさの上限は呼び出し側が
 * 決め、超えた時点で打ち切る (Content-Length が来ていれば受け取る前に断る)。
 */

define('REMOTE_MAX_REDIRECTS', 3);

/*
 * $url を GET する。本文は $to_file を渡せばそこへ、渡さなければ 'body' に入れる。
 *
 * 成功: array('status' => 応答コード, 'headers' => 小文字の名前 => 値,
 *             'body' => 本文, 'size' => バイト数)
 * 失敗: array('error' => 理由, 'host' => ホスト名)。理由は
 *       disabled   allow_url_fopen が無効
 *       url        http / https の URL でない
 *       private    プライベートなネットワークのホスト (許可の一覧に無い)
 *       resolve    名前を引けない
 *       connect    接続できない、または途中で時間切れ
 *       too_large  $max_bytes を超える
 *       redirect   転送が多すぎる
 */
function remote_fetch($url, $max_bytes, $to_file = null, $allowed_hosts = array()) {
    if(!ini_get('allow_url_fopen'))
	return array('error' => 'disabled', 'host' => '');

    for($i = 0; $i <= REMOTE_MAX_REDIRECTS; $i++) {
	$target = remote_target($url, $allowed_hosts);
	if(isset($target['error']))
	    return $target;

	$fp = @fopen($target['connect_url'], 'rb', false, stream_context_create($target['context']));
	if($fp === false)
	    return array('error' => 'connect', 'host' => $target['host']);
	stream_set_timeout($fp, EXTERNAL_FETCH_TIMEOUT);
	$meta = stream_get_meta_data($fp);
	list($status, $headers) = remote_parse_headers(default_value($meta['wrapper_data'], array()));

	if($status >= 300 && $status < 400 && isset($headers['location'])) {
	    fclose($fp);
	    $url = remote_absolute_url($url, $headers['location']);
	    continue;
	}

	if(isset($headers['content-length']) && (int)$headers['content-length'] > $max_bytes) {
	    fclose($fp);
	    return array('error' => 'too_large', 'host' => $target['host']);
	}

	$result = remote_read($fp, $max_bytes, $to_file);
	fclose($fp);
	if(isset($result['error'])) {
	    $result['host'] = $target['host'];
	    return $result;
	}
	$result['status'] = $status;
	$result['headers'] = $headers;
	return $result;
    }
    return array('error' => 'redirect', 'host' => $target['host']);
}

/* 本文を受け取る。$max_bytes を超えたら打ち切る */
function remote_read($fp, $max_bytes, $to_file) {
    $wfp = null;
    if(!is_null($to_file)) {
	$wfp = @fopen($to_file, 'wb');
	if($wfp === false)
	    return array('error' => 'connect');
    }
    $body = '';
    $size = 0;
    $error = null;
    while(!feof($fp)) {
	$data = fread($fp, CONTENTS_READ_BUFFER_SIZE);
	if($data === false || ($data === '' && stream_get_meta_data($fp)['timed_out'])) {
	    $error = 'connect';
	    break;
	}
	$size += strlen($data);
	if($size > $max_bytes) {
	    $error = 'too_large';
	    break;
	}
	if(is_null($wfp))
	    $body .= $data;
	else
	    fwrite($wfp, $data);
    }
    if(!is_null($wfp))
	fclose($wfp);
    if(!is_null($error))
	return array('error' => $error);
    return array('body' => $body, 'size' => $size);
}

/*
 * 接続先を決める。許可の一覧にあるホストは名前のまま接続する。
 * それ以外は名前を引き、どのアドレスも公開のものであることを確かめてから、
 * そのアドレスに接続する。
 */
function remote_target($url, $allowed_hosts) {
    $parts = @parse_url($url);
    if($parts === false || empty($parts['scheme']) || empty($parts['host']) ||
       isset($parts['user']) || isset($parts['pass']))
	return array('error' => 'url', 'host' => '');
    $scheme = strtolower($parts['scheme']);
    if($scheme !== 'http' && $scheme !== 'https')
	return array('error' => 'url', 'host' => '');

    $host = strtolower(trim($parts['host'], '[]'));
    $port = isset($parts['port']) ? (int)$parts['port'] : ($scheme === 'https' ? 443 : 80);
    $path = default_value($parts['path'], '/');
    if(isset($parts['query']))
	$path .= '?' . $parts['query'];

    $http = array(
	'method' => 'GET',
	'timeout' => EXTERNAL_FETCH_TIMEOUT,
	'follow_location' => 0,
	'ignore_errors' => true,
	'header' => 'User-Agent: NextForm/' . NEXTFORM_VERSION . "\r\n");

    if(in_array($host, $allowed_hosts, true)) {
	$connect_host = $host;
    } else {
	$addresses = remote_resolve($host);
	if(empty($addresses))
	    return array('error' => 'resolve', 'host' => $host);
	foreach($addresses as $address) {
	    if(filter_var($address, FILTER_VALIDATE_IP, FILTER_FLAG_GLOBAL_RANGE) === false)
		return array('error' => 'private', 'host' => $host);
	}
	$connect_host = $addresses[0];
	$host_header = (strpos($host, ':') !== false ? '[' . $host . ']' : $host);
	if(isset($parts['port']))
	    $host_header .= ':' . $port;
	$http['header'] = 'Host: ' . $host_header . "\r\n" . $http['header'];
    }

    $context = array('http' => $http);
    if($scheme === 'https')
	$context['ssl'] = array('peer_name' => $host, 'verify_peer' => true, 'verify_peer_name' => true);
    if(strpos($connect_host, ':') !== false)
	$connect_host = '[' . $connect_host . ']';
    return array(
	'host' => $host,
	'connect_url' => $scheme . '://' . $connect_host . ':' . $port . $path,
	'context' => $context);
}

/* 名前を引く。IPv4 を先に並べる (IPv6 で外に出られないサーバーがあるため) */
function remote_resolve($host) {
    if(filter_var($host, FILTER_VALIDATE_IP) !== false)
	return array($host);
    $addresses = @gethostbynamel($host);
    if(!empty($addresses))
	return $addresses;
    $addresses = array();
    $records = @dns_get_record($host, DNS_AAAA);
    if(is_array($records)) {
	foreach($records as $record) {
	    if(!empty($record['ipv6']))
		$addresses[] = $record['ipv6'];
	}
    }
    return $addresses;
}

/* 応答の行から、応答コードと「小文字の名前 => 値」を取り出す */
function remote_parse_headers($lines) {
    $status = 0;
    $headers = array();
    foreach($lines as $line) {
	if(preg_match('|^HTTP/\S+\s+(\d{3})|', $line, $matches)) {
	    $status = (int)$matches[1];
	    $headers = array();
	} else if(($pos = strpos($line, ':')) !== false) {
	    $headers[strtolower(trim(substr($line, 0, $pos)))] = trim(substr($line, $pos + 1));
	}
    }
    return array($status, $headers);
}

/* 転送先 (Location) を、もとの URL を基準に絶対 URL にする */
function remote_absolute_url($base, $location) {
    if(preg_match('|^[a-zA-Z][a-zA-Z0-9+.-]*://|', $location))
	return $location;
    $parts = parse_url($base);
    $origin = $parts['scheme'] . '://' . $parts['host'] . (isset($parts['port']) ? ':' . $parts['port'] : '');
    if(strpos($location, '//') === 0)
	return $parts['scheme'] . ':' . $location;
    if(strpos($location, '/') === 0)
	return $origin . $location;
    $directory = preg_replace('|[^/]*$|', '', default_value($parts['path'], '/'));
    return $origin . $directory . $location;
}
?>
