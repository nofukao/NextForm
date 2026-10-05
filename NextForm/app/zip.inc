<?php
/*
 * 圧縮しない ZIP を組み立てて送る。添付画面のダウンロード (option/attach.inc) が使う。
 *
 * PHP の zip 拡張は要求しない (project-overview.md §7)。共用サーバーにあるとも
 * 限らない。送るのは添付ファイルで、画像や PDF はもともと圧縮されているので、
 * 圧縮しなくても大きさはほぼ変わらない。
 *
 * 中身は 2 回読む。1 回目で CRC と大きさを数え、2 回目で送る。先に数えておけば
 * ヘッダに正しい値を書け、Content-Length も出せる (大きさを後ろに書く
 * データ記述子の形は、展開する道具によっては読めない)。どちらもストリームで
 * 読むので、ファイルが大きくてもメモリを使い込まない。
 *
 * 名前は UTF-8 の印 (汎用フラグのビット 11) を付けて入れる。付けないと、
 * 展開する側が CP437 や CP932 として読んで日本語の名前が化ける。
 * ZIP64 には対応しないので、65,535 個と 4GB を超えるものは作らない。
 *
 * $entries は次の形の配列の並び。
 *   'name'  ZIP の中の名前 (UTF-8)
 *   'mtime' 更新時刻
 *   'open'  中身のストリームを開く関数 (失敗したら false を返す)
 */

define('ZIP_MAX_ENTRIES', 0xffff);
define('ZIP_MAX_BYTES', 0xffffffff);

/*
 * 中身を読んで CRC と大きさを数え、$entries に書き込む。
 * ZIP の全体の大きさを返す。開けないものがあるか、大きすぎて ZIP に
 * 入らないときは false。
 */
function zip_prepare(&$entries) {
    if(count($entries) > ZIP_MAX_ENTRIES)
	return false;

    $offset = 0;
    $directory_size = 0;
    foreach($entries as &$entry) {
	$fp = call_user_func($entry['open']);
	if($fp === false)
	    return false;
	$hash = hash_init('crc32b');
	$size = 0;
	while(!feof($fp)) {
	    $data = fread($fp, CONTENTS_READ_BUFFER_SIZE);
	    if($data === false)
		break;
	    hash_update($hash, $data);
	    $size += strlen($data);
	}
	fclose($fp);

	$entry['crc'] = hexdec(hash_final($hash));
	$entry['size'] = $size;
	$entry['offset'] = $offset;
	$offset += 30 + strlen($entry['name']) + $size;
	$directory_size += 46 + strlen($entry['name']);
    }
    unset($entry);

    $total = $offset + $directory_size + 22;
    if($total > ZIP_MAX_BYTES)
	return false;
    return $total;
}

/* zip_prepare() を通した $entries を送る */
function zip_output(&$entries) {
    $directory = '';
    foreach($entries as &$entry) {
	list($time, $date) = zip_dos_time($entry['mtime']);
	$name_length = strlen($entry['name']);

	echo(pack('VvvvvvVVVvv',
		  0x04034b50,		// ローカルファイルヘッダ
		  20,			// 展開に要る版 (2.0)
		  0x0800,		// 名前は UTF-8
		  0,			// 圧縮しない
		  $time, $date,
		  $entry['crc'], $entry['size'], $entry['size'],
		  $name_length, 0) . $entry['name']);

	$fp = call_user_func($entry['open']);
	if($fp !== false) {
	    while(!feof($fp))
		echo(fread($fp, CONTENTS_READ_BUFFER_SIZE));
	    fclose($fp);
	}

	$directory .= pack('VvvvvvvVVVvvvvvVV',
			   0x02014b50,		// セントラルディレクトリ
			   20, 20,		// 作った版、展開に要る版
			   0x0800, 0,
			   $time, $date,
			   $entry['crc'], $entry['size'], $entry['size'],
			   $name_length, 0, 0,	// 名前、拡張、コメントの長さ
			   0, 0, 0,		// ディスク番号、内部属性、外部属性
			   $entry['offset']) . $entry['name'];
    }
    unset($entry);

    $directory_offset = 0;
    if(!empty($entries)) {
	$last = end($entries);
	$directory_offset = $last['offset'] + 30 + strlen($last['name']) + $last['size'];
    }
    echo($directory);
    echo(pack('VvvvvVVv',
	      0x06054b50,		// 終端
	      0, 0,
	      count($entries), count($entries),
	      strlen($directory), $directory_offset,
	      0));
}

/* ZIP の日時は MS-DOS の形 (2 秒刻み、1980 年から) */
function zip_dos_time($mtime) {
    $t = getdate($mtime);
    if($t['year'] < 1980)
	return array(0, (1 << 5) | 1);
    return array(($t['hours'] << 11) | ($t['minutes'] << 5) | intdiv($t['seconds'], 2),
		 (($t['year'] - 1980) << 9) | ($t['mon'] << 5) | $t['mday']);
}
?>
