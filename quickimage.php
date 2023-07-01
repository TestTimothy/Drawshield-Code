<?php /* Copyright 2010-2021 Karl Wilcox, Mattias Basaglia

This file is part of the DrawShield.net heraldry image creation program

    DrawShield is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

     DrawShield is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with  DrawShield.  If not, see https://www.gnu.org/licenses/.
 */

//
// Global Variables
//
$options = array();
include 'version.inc';
include 'parser/utilities.inc';
include 'analyser/utilities.inc';
/**
 * @var DOMDocument $dom
 */
$dom = null;
/**
 * @var DOMXpath $xpath
 */
$xpath = null;
/**
 * @var messageStore $messages
 */
$messages = null;

// Process arguments
if (isset($_GET['blazon'])) $options['blazon'] = html_entity_decode(strip_tags(trim($_GET['blazon'])));
if (isset($_GET['outputformat'])) $options['outputFormat'] = strip_tags ($_GET['outputformat']); else $options['outputFormat'] = 'png';
if (isset($_GET['palette'])) $options['palette'] = strip_tags($_GET['palette']);
if (isset($_GET['effect'])) $options['effect'] = strip_tags($_GET['effect']);
if (isset($_GET['nomask'])) $options['nomask'] = true;
if (isset($_GET['raw'])) $options['raw'] = true;
if (isset($_GET['size'])) {
  $size = strip_tags ($_GET['size']);
  if ( $size < 100 ) $size = 100;
  $options['size'] = $size;
}
$options['asFile'] = "1";

  include "parser/parser.inc";
  $p = new parser('english');
  $dom = $p->parse($options['blazon'],'dom');
  $p = null; // destroy parser
  // Resolve references
  include "analyser/references.inc";
  $references = new references($dom);
  $dom = $references->setReferences();
  $references = null; // destroy references

  // Read in the drawing code  ( All formats start out as SVG )
  $xpath = new DOMXPath($dom);
  include "svg/draw.inc";
  $output = draw();
  switch ($options['outputFormat']) {
    case 'jpg':
      $im = new Imagick();
      $im->readimageblob($output);
      $im->setimageformat('jpeg');
      $im->setimagecompressionquality(90);
      // $im->scaleimage(1000,1200);
      header('Content-Type: image/jpg');
      echo $im->getimageblob();
      break;
    case 'png-old':
      $im = new Imagick();
      $im->readimageblob($output);
      $im->setimageformat('png');
      // $im->paintTransparentImage('white',0.0,1000.0);
      // $im->scaleimage(1000,1200);
      header('Content-Type: image/png');
       echo $im->getimageblob();
      break;
      case 'png':
        $dir = sys_get_temp_dir();
        $base = tempnam($dir, 'shield');
        rename($base, $base . '.svg');
        file_put_contents($base . '.svg', $output);
        $result = shell_exec("java -jar /opt/bitnami/apache/etc/batik/batik-rasterizer-1.16.jar $base.svg -d $base.png");
        unlink($base . '.svg');
        header('Content-Type: image/png');
        echo file_get_contents($base . '.png');
        unlink($base . '.png');
        break;
        default:
    case 'svg':
      header('Content-Type: text/xml; charset=utf-8');
      echo $output;
      break;
}

